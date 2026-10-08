#include "SPStealthObserverComponent.h"
#include "SPStealthActivityComponent.h"
#include "SPRestrictedArea.h"
#include "SPGuardAlertSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

USPStealthObserverComponent::USPStealthObserverComponent() { PrimaryComponentTick.bCanEverTick=true; }
bool USPStealthObserverComponent::CanObserve() const
{
    const auto* S=GetWorld()?GetWorld()->GetSubsystem<USPGuardAlertSubsystem>():nullptr;
    return bObservationEnabled && GetOwner() && GetOwner()->HasAuthority() && IsRegistered() && S && S->IsStageActive();
}
bool USPStealthObserverComponent::TestVisibility(const AActor* Observer,const FTransform& Eye,const APawn* Player,const FSPStealthSightSettings& Settings)
{
    if (!IsValid(Observer) || !IsValid(Player) || Player==Observer || !Player->IsPlayerControlled() || Player->GetWorld()!=Observer->GetWorld()) { return false; }
    const auto* Activity=Player->FindComponentByClass<USPStealthActivityComponent>();
    if (Activity && Activity->IsIncapacitated()) { return false; }
    const auto* Capsule=Player->FindComponentByClass<UCapsuleComponent>();
    const float Height=Capsule?Capsule->GetScaledCapsuleHalfHeight():0;
    // 벽·천장 중력에서도 몸통을 검사하도록 월드 Z축 대신 실제 캡슐의 위쪽 축을 사용한다.
    // 관찰자의 시야 각도는 아래에서 별도로 눈의 로컬 좌표로 계산한다.
    const FVector Center=Capsule?Capsule->GetComponentLocation():Player->GetActorLocation();
    const FVector Up=Capsule?Capsule->GetUpVector():Player->GetActorUpVector();
    const FVector Samples[]={Center+Up*Height*0.65f,Center};
    FCollisionQueryParams Params(SCENE_QUERY_STAT(StealthObserverSight),false,Observer);
    Params.AddIgnoredActor(Player);
    for (const FVector& Point:Samples)
    {
        const FVector Delta=Point-Eye.GetLocation();
        if (Delta.SizeSquared()>FMath::Square(FMath::Max(Settings.Distance,0.0f))) { continue; }
        const FVector Local=Eye.InverseTransformVectorNoScale(Delta);
        const float Horizontal=FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)));
        const float Vertical=FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Local.Z,Local.Size2D())));
        if (Horizontal>FMath::Clamp(Settings.HorizontalAngle,1.0f,179.0f)*0.5f
            || Vertical>FMath::Clamp(Settings.VerticalAngle,1.0f,179.0f)*0.5f) { continue; }
        FHitResult Hit;
        if (!Observer->GetWorld()->LineTraceSingleByChannel(Hit,Eye.GetLocation(),Point,ECC_Visibility,Params)) { return true; }
    }
    return false;
}
bool USPStealthObserverComponent::CanSeePlayer(const APawn* Player) const
{
    RefreshSettings.ExecuteIfBound();
    return bObservationEnabled && TestVisibility(GetOwner(),GetComponentTransform(),Player,Sight);
}
void USPStealthObserverComponent::Emit(APawn* Player,ESPCrimeKind Kind,bool bRestricted,bool bInstant,const FGuid& ActionId)
{
    FSPStealthWitness W;
    W.WitnessId=FGuid::NewGuid(); W.ActionId=ActionId; W.Player=Player;
    W.Location=Player->GetActorLocation(); W.Crime=Kind; W.bRestricted=bRestricted; W.bInstant=bInstant;
    const auto* GS=GetWorld()->GetGameState();
    W.ServerTime=GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    // 여기서는 증거만 전달한다. 신고 지연·즉시 추격 같은 대응은 경비/CCTV/민간인별 수신부에서 확장한다.
    OnWitnessConfirmedNative.Broadcast(W);
    OnWitnessConfirmed.Broadcast(W);
}
FSPStealthObservation USPStealthObserverComponent::SamplePlayer(APawn* Player,float DeltaSeconds)
{
    FSPStealthObservation R;
    if (!CanObserve()) { Confirmations.Reset(); return R; }
    // 접속 종료·Pawn 교체 후 예전 대상에게 쌓인 부분 확인 시간이 남지 않게 한다.
    for (auto It=Confirmations.CreateIterator();It;++It)
    { if (!It.Key().IsValid() || !It.Key()->IsPlayerControlled()) { It.RemoveCurrent(); } }
    R.bVisible=CanSeePlayer(Player);
    // 시야가 끊기면 이 관찰자와 해당 플레이어 사이의 누적 시간만 초기화한다.
    if (!R.bVisible) { Confirmations.Remove(Player); return R; }
    R.bRestricted=ASPRestrictedArea::FindAtLocation(this,Player->GetActorLocation())!=nullptr;
    uint32 Revision=0;
    if (const auto* Activity=Player->FindComponentByClass<USPStealthActivityComponent>())
    { R.bContinuousCrime=Activity->HasContinuousCrime(R.Crime); Revision=Activity->GetCrimeRevision(); }
    if (!R.bRestricted && !R.bContinuousCrime) { Confirmations.Remove(Player); return R; }
    FConfirmation& C=Confirmations.FindOrAdd(Player);
    // 일반 구역에서는 취소 후 새 작업을 시작하면 다시 확인해야 한다.
    // 제한 구역 안에서는 작업 변경과 무관하게 '계속 머무름' 자체가 확인 대상이다.
    if (C.bRestricted!=R.bRestricted || (!R.bRestricted && C.CrimeRevision!=Revision)) { C=FConfirmation(); }
    C.bRestricted=R.bRestricted; C.CrimeRevision=Revision;
    C.Seconds+=FMath::Max(DeltaSeconds,0.0f);
    R.Progress=FMath::Clamp(C.Seconds/FMath::Max(Sight.ConfirmationSeconds,0.1f),0.0f,1.0f);
    R.bConfirmed=R.Progress>=1;
    if (R.bConfirmed && !C.bEmitted)
    {
        C.bEmitted=true;
        Emit(Player,R.Crime,R.bRestricted,false);
    }
    return R;
}
void USPStealthObserverComponent::ObserveInstantCrime(APawn* Player,ESPCrimeKind Kind,const FGuid& ActionId)
{
    if (!CanObserve() || Kind==ESPCrimeKind::None || !ActionId.IsValid() || InstantActions.Contains(ActionId)) { return; }
    // 보이지 않았던 행동도 소비한다. 같은 ID를 나중에 재전송해 과거 범죄를 뒤늦게 목격하게 만들지 않는다.
    InstantActions.Add(ActionId);
    if (CanSeePlayer(Player)) { Emit(Player,Kind,ASPRestrictedArea::FindAtLocation(this,Player->GetActorLocation())!=nullptr,true,ActionId); }
}
void USPStealthObserverComponent::ResetObservations() { Confirmations.Reset(); InstantActions.Reset(); }
void USPStealthObserverComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    if (!bAutoObserve) { return; }
    if (!CanObserve()) { ResetObservations(); return; }
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    { if (const auto* PC=It->Get()) { SamplePlayer(PC->GetPawn(),DeltaTime); } }
}
