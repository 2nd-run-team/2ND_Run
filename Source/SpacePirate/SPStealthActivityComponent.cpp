#include "SPStealthActivityComponent.h"
#include "SPRestrictedArea.h"
#include "SPStealthObserverComponent.h"
#include "SPGuardAlertSubsystem.h"
#include "SPStealthGameStateComponent.h"
#include "SPInteractableComponent.h"
#include "SPInteractorComponent.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

USPStealthActivityComponent::USPStealthActivityComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick=true;
    PrimaryComponentTick.TickInterval=0.05f;
}
void USPStealthActivityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USPStealthActivityComponent,bIncapacitated);
    DOREPLIFETIME(USPStealthActivityComponent,CurrentArea);
}
bool USPStealthActivityComponent::CanRegister(UObject* Source) const
{
    const APawn* Pawn=GetOwner<APawn>();
    const auto* S=GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    return Pawn && Pawn->HasAuthority() && Pawn->IsPlayerControlled() && !bIncapacitated && !bCancelling
        && IsValid(Source) && Source->GetWorld()==GetWorld() && S && S->IsStageActive();
}
bool USPStealthActivityComponent::IsRegistrationValid(const FRegistration& R) const
{
    const auto* Component=Cast<UActorComponent>(R.Source.Get());
    const auto* Actor=Component?Component->GetOwner():Cast<AActor>(R.Source.Get());
    const auto* S=GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    return CanRegister(R.Source.Get()) && (!Actor || (IsValid(Actor) && !Actor->IsActorBeingDestroyed()))
        && (!Component || Component->IsRegistered()) && S->GetSecurityState()->GetAlarmState().StageId==R.StageId;
}
FGuid USPStealthActivityComponent::BeginCrime(ESPCrimeKind Kind,UObject* Source)
{
    if (Kind==ESPCrimeKind::None || !CanRegister(Source)) { return FGuid(); }
    // 같은 생산자의 중복 시작은 기존 핸들을 돌려준다. 호출 빈도가 작업 수를 늘리면 안 된다.
    for (const auto& Entry:Registrations)
    { if (Entry.Value.Source==Source && Entry.Value.Kind==Kind && IsRegistrationValid(Entry.Value)) { return Entry.Key; } }
    const FGuid Id=FGuid::NewGuid();
    const auto* S=GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    Registrations.Add(Id,{Kind,Source,S->GetSecurityState()->GetAlarmState().StageId});
    ++CrimeRevision;
    return Id;
}
void USPStealthActivityComponent::EndCrime(FGuid Handle)
{
    if (GetOwner()->HasAuthority() && Registrations.Remove(Handle)>0) { ++CrimeRevision; }
}
TArray<ESPCrimeKind> USPStealthActivityComponent::GetActiveCrimes() const
{
    TArray<ESPCrimeKind> Result;
    for (const auto& Entry:Registrations) { if (IsRegistrationValid(Entry.Value)) { Result.AddUnique(Entry.Value.Kind); } }
    Result.Sort([](ESPCrimeKind A,ESPCrimeKind B){return uint8(A)<uint8(B);});
    return Result;
}
bool USPStealthActivityComponent::HasContinuousCrime(ESPCrimeKind& OutKind) const
{
    const auto Active=GetActiveCrimes();
    OutKind=Active.IsEmpty()?ESPCrimeKind::None:Active[0];
    return !Active.IsEmpty();
}
FGuid USPStealthActivityComponent::ReportInstantCrime(ESPCrimeKind Kind,UObject* Source)
{
    if (Kind==ESPCrimeKind::None || !CanRegister(Source)) { return FGuid(); }
    const FGuid ActionId=FGuid::NewGuid();
    // 순간 행동은 지금 존재하는 관찰자에게 지금의 시야로 알린다.
    // 콜백이 액터를 생성/파괴할 수 있으므로 먼저 약한 참조 목록을 만들고 유효성을 재확인한다.
    TArray<TWeakObjectPtr<USPStealthObserverComponent>> Observers;
    for (TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<USPStealthObserverComponent*> Components; It->GetComponents(Components);
        for (auto* Observer:Components) { Observers.Add(Observer); }
    }
    for (auto Observer:Observers)
    { if (Observer.IsValid()) { Observer->ObserveInstantCrime(GetOwner<APawn>(),Kind,ActionId); } }
    return ActionId;
}
void USPStealthActivityComponent::CancelAllActivity()
{
    if (!GetOwner()->HasAuthority() || bCancelling) { return; }
    TGuardValue<bool> Cancelling(bCancelling,true);
    const auto Previous=Registrations;
    Registrations.Reset(); // 취소 콜백이 재진입하기 전에 비우고, bCancelling으로 새 작업 등록도 막는다.
    ++CrimeRevision;
    for (const auto& Entry:Previous)
    { if (auto* Interaction=Cast<USPInteractableComponent>(Entry.Value.Source.Get())) { Interaction->CancelBy(GetOwner<APawn>()); } }
    if (auto* Interactor=GetOwner()->FindComponentByClass<USPInteractorComponent>()) { Interactor->StopInteract(); }
}
void USPStealthActivityComponent::SetIncapacitated(bool bValue)
{
    if (!GetOwner()->HasAuthority()) { return; }
    bIncapacitated=bValue;
    if (bValue) { CancelAllActivity(); }
    GetOwner()->ForceNetUpdate();
}
// 보안만 초기화할 때 체력 소유자의 다운 상태를 해제하면 안 된다. 회복은 상태 이벤트로만 전달받는다.
void USPStealthActivityComponent::ResetForStage() { CancelAllActivity(); }
void USPStealthActivityComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
    if (!GetOwner()->HasAuthority()) { return; }
    APawn* Pawn=GetOwner<APawn>();
    if (!Pawn || !Pawn->IsPlayerControlled() || bIncapacitated) { if (!Registrations.IsEmpty()) { CancelAllActivity(); } }
    for (auto It=Registrations.CreateIterator();It;++It) { if (!IsRegistrationValid(It.Value())) { It.RemoveCurrent(); } }
    auto* Area=Pawn && Pawn->IsPlayerControlled()?ASPRestrictedArea::FindAtLocation(this,Pawn->GetActorLocation()):nullptr;
    if (CurrentArea!=Area) { CurrentArea=Area; GetOwner()->ForceNetUpdate(); }
}
void USPStealthActivityComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    CancelAllActivity();
    Super::EndPlay(Reason);
}
