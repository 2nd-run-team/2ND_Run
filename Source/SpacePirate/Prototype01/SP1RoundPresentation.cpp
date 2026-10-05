// 작성자 : 임진혁
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1CarRegion.h"
#include "Prototype01/SP1TransferCargo.h"
#include "Prototype01/SP1CargoDefinition.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "Prototype01/SP1LaserHazard.h"
#include "Prototype01/SP1SecurityCamera.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

int32 USP1RoundComponent::RemainingCarValue(FName Id) const
{
    int32 Value = 0;
    for (const ASP1TransferCargo* Item : Cargo)
        if (IsValid(Item) && Item->CarId == Id && !Item->bTransferred && Item->Definition) Value += Item->Definition->Value;
    return Value;
}

void USP1RoundComponent::UpdatePresentationState()
{
    const double Time = Now();
    const int32 Removed = Pings.RemoveAll([this,Time](const FSP1Ping& Ping)
    {
        const auto* P = Participants.FindByPredicate([&](const FSP1Participant& Row){return Row.PlayerId == Ping.PlayerId;});
        return Ping.ExpiresAt <= Time || !P || !P->bConnected;
    });
    if (Removed) GetOwner()->ForceNetUpdate();
    for (auto& P : Participants)
    {
        const auto* PC = FindController(P.PlayerId);
        const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        const auto* Life = Pawn ? Pawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
        const auto* Region = Pawn && P.bAlive ? ASP1CarRegion::FindAt(this,Pawn->GetActorLocation()) : nullptr;
        P.CarId = Region ? Region->CarId : NAME_None;
        P.Wounds = Life ? Life->Wounds : P.Wounds;
        if (!SP1::IsPlaying(State.Phase)) continue;
        FName& Before = PreviousCars.FindOrAdd(P.PlayerId);
        if (Before == P.CarId) continue;
        if (!Before.IsNone()) LogEvent(TEXT("CarExit"),nullptr,NAME_None,P.bAlive ? TEXT("Moved") : TEXT("DeadOrDisconnected"),RemainingCarValue(Before),Before,P.PlayerId);
        if (!P.CarId.IsNone()) LogEvent(TEXT("CarEnter"),nullptr,NAME_None,TEXT("Entered"),RemainingCarValue(P.CarId),P.CarId,P.PlayerId);
        Before = P.CarId;
    }
}

void USP1RoundComponent::CloseCarVisits(const FString& Reason)
{
    for (const auto& Pair : PreviousCars)
        if (!Pair.Value.IsNone()) LogEvent(TEXT("CarExit"),nullptr,NAME_None,Reason,RemainingCarValue(Pair.Value),Pair.Value,Pair.Key);
    PreviousCars.Reset();
}

void USP1RoundComponent::RequestPing(USP1InteractionComponent* Source, const FGuid& RunId)
{
    if (!GetOwner()->HasAuthority() || !Source || RunId != State.RunId || !CanInteract(Cast<APawn>(Source->GetOwner()))) return;
    APlayerController* PC = Source->GetPlayerController();
    if (!PC || !PC->PlayerState) return;
    const int32 PlayerId = PC->PlayerState->GetPlayerId();
    const double Time = Now();
    double& Next = NextPingAt.FindOrAdd(PlayerId,-1.);
    if (Time < Next) return;
    Next = Time + 1.; // 실패한 요청도 서버에서 재사용 제한한다.

    FVector Eye; FRotator View;
    PC->GetPlayerViewPoint(Eye,View);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SP1Ping),false,Source->GetOwner());
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Eye+View.Vector()*3000.f,ECC_Visibility,Query)) return;
    // 최초 가림 표면만 고정한다. 클라이언트가 위치·종류·대상 Actor를 확정하지 않는다.
    Pings.RemoveAll([Time](const FSP1Ping& P){return P.ExpiresAt <= Time;});
    TArray<int32> Own;
    for (int32 I=0; I<Pings.Num(); ++I) if (Pings[I].PlayerId == PlayerId) Own.Add(I);
    while (Own.Num() >= 2)
    {
        Pings.RemoveAt(Own[0]); Own.Reset();
        for (int32 I=0; I<Pings.Num(); ++I) if (Pings[I].PlayerId == PlayerId) Own.Add(I);
    }
    FSP1Ping Ping; Ping.Id=++NextPingId; Ping.PlayerId=PlayerId; Ping.Location=Hit.ImpactPoint;
    Ping.ExpiresAt=Time+8.;
    if (Cast<ASP1TransferCargo>(Hit.GetActor())) Ping.Kind=ESP1PingKind::Cargo;
    else if (Cast<ASP1LaserHazard>(Hit.GetActor()) || Cast<ASP1SecurityCamera>(Hit.GetActor())) Ping.Kind=ESP1PingKind::Danger;
    Pings.Add(Ping);
    LogEvent(TEXT("Ping"),Source,NAME_None,FString::Printf(TEXT("id=%d kind=%d fixed=(%.1f,%.1f,%.1f)"),Ping.Id,static_cast<int32>(Ping.Kind),Ping.Location.X,Ping.Location.Y,Ping.Location.Z));
    GetOwner()->ForceNetUpdate();
}
