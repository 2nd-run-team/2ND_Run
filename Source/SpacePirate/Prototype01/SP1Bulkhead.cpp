// 작성자 : 임진혁
#include "Prototype01/SP1Bulkhead.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ASP1Panel::ASP1Panel()
{
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetSimulatePhysics(false);
}
FVector ASP1Panel::GetInteractionPoint() const { return GetActorTransform().TransformPosition(InteractionOffset); }
ASP1Bulkhead::ASP1Bulkhead()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
    SetRootComponent(Barrier);
    Barrier->SetBoxExtent(FVector(15,250,210));
    Barrier->SetCollisionProfileName(TEXT("BlockAll"));
    DoorLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorLeft"));
    DoorRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorRight"));
    for (auto* Door : {DoorLeft.Get(), DoorRight.Get()})
    {
        Door->SetupAttachment(Barrier);
        Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}
void ASP1Bulkhead::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASP1Bulkhead, bOpen);
    DOREPLIFETIME(ASP1Bulkhead, LeftHolder);
    DOREPLIFETIME(ASP1Bulkhead, RightHolder);
    DOREPLIFETIME(ASP1Bulkhead, BypassHolder);
    DOREPLIFETIME(ASP1Bulkhead, ProgressStartedAt);
}
FName ASP1Bulkhead::TryClaim(USP1InteractionComponent* Source, const ASP1Panel* Panel)
{
    if (!HasAuthority() || !Source || !Panel || Panel->Bulkhead != this || bOpen) return TEXT("DeviceUnavailable");
    APawn* Pawn = Cast<APawn>(Source->GetOwner());
    if (!Pawn) return TEXT("NotAlive");
    // 서버 요청 순서에서 경로를 점유한다. 협동/단독 시간이 서로 이어지거나 동시에 완료되지 않는다.
    if (Panel->Kind == ESP1PanelKind::Bypass)
    {
        if (LeftHolder || RightHolder || BypassHolder) return TEXT("PanelsInUse");
        BypassHolder = Pawn;
        ProgressStartedAt = USP1RoundComponent::Find(this)->Now();
    }
    else
    {
        if (BypassHolder) return TEXT("BypassInUse");
        auto& Slot = Panel->Kind == ESP1PanelKind::Left ? LeftHolder : RightHolder;
        if (Slot || LeftHolder == Pawn || RightHolder == Pawn) return TEXT("DifferentPlayersRequired");
        Slot = Pawn;
        if (LeftHolder && RightHolder) ProgressStartedAt = USP1RoundComponent::Find(this)->Now();
    }
    ForceNetUpdate();
    return NAME_None;
}
void ASP1Bulkhead::Release(USP1InteractionComponent* Source)
{
    if (!HasAuthority() || !Source) return;
    bool bChanged = false;
    for (auto* Slot : {&LeftHolder, &RightHolder, &BypassHolder})
        if (*Slot == Source->GetOwner()) { *Slot = nullptr; bChanged = true; }
    if (bChanged) { ProgressStartedAt = -1; ForceNetUpdate(); }
}
USP1InteractionComponent* ASP1Bulkhead::ValidHolder(APawn* Pawn, double Time, bool bStrict) const
{
    auto* Source = IsValid(Pawn) ? Pawn->FindComponentByClass<USP1InteractionComponent>() : nullptr;
    auto* Panel = Source ? Cast<ASP1Panel>(Source->Attempt.Target) : nullptr;
    return Source && Panel && Panel->Bulkhead == this && Source->ValidateDeviceHold(Time,bStrict) ? Source : nullptr;
}
float ASP1Bulkhead::GetProgress(double Time) const
{
    if (bOpen) return 1;
    return ProgressStartedAt >= 0 ? FMath::Clamp(float((Time-ProgressStartedAt) / FMath::Max(.2f, BypassHolder ? BypassSeconds : TogetherSeconds)),0.f,1.f) : 0;
}
void ASP1Bulkhead::EvaluateOnServer(double Time)
{
    if (!HasAuthority() || bOpen) return;
    const bool bDue = GetProgress(Time) >= 1;
    auto* Left = ValidHolder(LeftHolder,Time,bDue);
    auto* Right = ValidHolder(RightHolder,Time,bDue);
    auto* Bypass = ValidHolder(BypassHolder,Time,bDue);
    if (!Left) LeftHolder = nullptr;
    if (!Right) RightHolder = nullptr;
    if (!Bypass) BypassHolder = nullptr;
    if (!Bypass && !(Left && Right && Left->GetPlayerController() != Right->GetPlayerController()))
    { ProgressStartedAt = -1; return; }
    if (!bDue || ProgressStartedAt < 0) return;
    bOpen = true;
    OnRep_Open();
    if (auto* Round = USP1RoundComponent::Find(this)) Round->LogEvent(TEXT("BulkheadOpened"), Bypass ? Bypass : Left, DeviceId, Bypass ? TEXT("Solo12s") : TEXT("Together2s"));
    for (auto* Source : {Left, Right, Bypass}) if (Source) Source->CancelOnServer(TEXT("Completed"));
    ForceNetUpdate();
}
void ASP1Bulkhead::ResetForRun()
{
    if (!HasAuthority()) return;
    bOpen = false; LeftHolder = nullptr; RightHolder = nullptr; BypassHolder = nullptr; ProgressStartedAt = -1;
    OnRep_Open(); ForceNetUpdate();
}
void ASP1Bulkhead::OnRep_Open()
{
    Barrier->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    DoorLeft->SetVisibility(!bOpen); DoorRight->SetVisibility(!bOpen);
}
