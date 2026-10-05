// 작성자 : 임진혁
#include "Prototype01/SP1LaserHazard.h"
#include "Prototype01/SP1SessionSubsystem.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASP1LaserHazard::ASP1LaserHazard()
{
    bReplicates = true; bAlwaysRelevant = true; PrimaryActorTick.bCanEverTick = true;
    DamageBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("DamageBounds"));
    SetRootComponent(DamageBounds);
    DamageBounds->SetBoxExtent(FVector(14,210,120));
    DamageBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DamageBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    DamageBounds->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    DamageBounds->SetGenerateOverlapEvents(true);
    Emitter = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Emitter")); Emitter->SetupAttachment(RootComponent);
    Receiver = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Receiver")); Receiver->SetupAttachment(RootComponent);
    Emitter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Receiver->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Indicator = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Indicator")); Indicator->SetupAttachment(RootComponent);
    Indicator->SetWorldSize(22); Indicator->SetHorizontalAlignment(EHTA_Center);
    Indicator->SetRelativeLocation(FVector(-30,0,145)); Indicator->SetRelativeRotation(FRotator(0,180,0));
    for (int32 Index=0; Index<4; ++Index)
    {
        auto* Beam = CreateDefaultSubobject<UNiagaraComponent>(*FString::Printf(TEXT("Beam%d"), Index));
        Beam->SetupAttachment(RootComponent); Beam->SetAutoActivate(false); Beams.Add(Beam);
    }
}
void ASP1LaserHazard::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    for (UNiagaraComponent* Beam : Beams) if (Beam) Beam->SetAsset(BeamSystem);
    UpdateBeamEndpoints();
    Indicator->SetText(FText::FromString(TEXT("LASER / OFF")));
}
void ASP1LaserHazard::UpdateBeamEndpoints()
{
    const FVector Extent = DamageBounds->GetUnscaledBoxExtent();
    for (int32 Index=0; Index<Beams.Num(); ++Index)
    {
        UNiagaraComponent* Beam = Beams[Index];
        if (!Beam) continue;
        const float Z = FMath::Lerp(-Extent.Z+20, Extent.Z-20, static_cast<float>(Index)/FMath::Max(1,Beams.Num()-1));
        Beam->SetRelativeLocation(FVector(0,-Extent.Y,Z));
        // 실제 원본 Niagara의 공개 변수 Color/Vector3f LaserEnd를 사용한다. 끝점은 월드 좌표다.
        Beam->SetVariableVec3(TEXT("User.LaserEnd"),GetActorTransform().TransformPosition(FVector(0,Extent.Y,Z)));
    }
}
void ASP1LaserHazard::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASP1LaserHazard, RunId); DOREPLIFETIME(ASP1LaserHazard, CycleEpoch);
}
void ASP1LaserHazard::StartCycle(const FGuid& NewRunId, double ServerTime)
{
    if (!HasAuthority()) return;
    RunId = NewRunId; CycleEpoch = ServerTime; NextDamageAt.Reset(); ForceNetUpdate();
}
ESP1LaserPhase ASP1LaserHazard::GetPhase() const
{
    const auto* Round = USP1RoundComponent::Find(this);
    if (!Round || Round->State.RunId != RunId || CycleEpoch < 0 || !SP1::IsPlaying(Round->State.Phase)) return ESP1LaserPhase::Off;
    return SP1::LaserPhase(Round->Now(),CycleEpoch,OnSeconds,OffSeconds,WarningSeconds);
}
void ASP1LaserHazard::EvaluateDamage(double Time)
{
    if (!HasAuthority() || GetPhase() != ESP1LaserPhase::On) return;
    TArray<AActor*> Actors; DamageBounds->GetOverlappingActors(Actors, APawn::StaticClass());
    for (AActor* Actor : Actors)
    {
        auto* Pawn = Cast<APawn>(Actor);
        auto* Life = Pawn ? Pawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
        if (!Life || Life->IsDead() || !Life->IsSimulationEnabled()) continue;
        double& Next = NextDamageAt.FindOrAdd(Pawn, -1);
        if (Time + UE_DOUBLE_SMALL_NUMBER < Next) continue;
        Next = Time + FMath::Max(0.05f,DamageInterval);
        Life->QueueWound(WoundAmount, HazardId);
    }
}
void ASP1LaserHazard::Tick(float Delta)
{
    Super::Tick(Delta);
    DisplayedPhase = GetPhase();
    if (LastVisualPhase == static_cast<int32>(DisplayedPhase)) return;
    LastVisualPhase = static_cast<int32>(DisplayedPhase);
    UpdateBeamEndpoints();
    const bool bOn = DisplayedPhase == ESP1LaserPhase::On;
    const bool bWarn = DisplayedPhase == ESP1LaserPhase::Warning;
    for (UNiagaraComponent* Beam : Beams)
    {
        Beam->SetVariableLinearColor(TEXT("User.Color"), bOn ? ActiveColor : WarningColor);
        if (bOn) Beam->Activate(true); else Beam->DeactivateImmediate();
    }
    Indicator->SetText(FText::FromString(bOn ? TEXT("LASER / ON - DANGER") : bWarn ? TEXT("LASER / WARNING") : TEXT("LASER / OFF")));
    Indicator->SetTextRenderColor((bOn ? ActiveColor : bWarn ? WarningColor : FLinearColor(0.1f,0.8f,0.4f)).ToFColor(true));
    if (IsValid(WarningAudio)) WarningAudio->Stop();
    if (bWarn && WarningSound && GetNetMode()!=NM_DedicatedServer && USP1SessionSubsystem::CanPlayAudio(this))
    {
        WarningAudio = UGameplayStatics::SpawnSoundAtLocation(this,WarningSound,GetActorLocation(),FRotator::ZeroRotator,0.25f,1,0,Attenuation);
        USP1SessionSubsystem::TrackAudio(this,WarningAudio);
    }
    if (HasAuthority()) if (auto* Round = USP1RoundComponent::Find(this))
        Round->LogEvent(TEXT("LaserPhase"),nullptr,HazardId,bOn ? TEXT("On") : bWarn ? TEXT("Warning") : TEXT("Off"));
}

void ASP1LaserHazard::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(WarningAudio)) WarningAudio->Stop();
    Super::EndPlay(Reason);
}
