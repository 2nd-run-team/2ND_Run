#include "SPGuardAlertSubsystem.h"
#include "SPGuardCharacter.h"
#include "SPStealthActivityComponent.h"
#include "SPStealthObserverComponent.h"
#include "SPStealthGameStateComponent.h"
#include "SPStealthPlayerStateComponent.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogSPStealth, Log, All);

bool USPGuardAlertSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}
bool USPGuardAlertSubsystem::IsServer() const { return GetWorld() && GetWorld()->GetNetMode() != NM_Client; }
USPStealthGameStateComponent* USPGuardAlertSubsystem::GetSecurityState() const
{
    const auto* GS = GetWorld()->GetGameState();
    return GS ? GS->FindComponentByClass<USPStealthGameStateComponent>() : nullptr;
}
USPStealthGameStateComponent* USPGuardAlertSubsystem::EnsureSecurityState()
{
    auto* GS = GetWorld()->GetGameState();
    if (!IsServer() || !IsValid(GS) || !GS->HasAuthority()) { return nullptr; }
    if (auto* State = GetSecurityState()) { return State; }
    auto* State = NewObject<USPStealthGameStateComponent>(GS);
    GS->AddInstanceComponent(State);
    State->RegisterComponent();
    GS->ForceNetUpdate();
    return State;
}
bool USPGuardAlertSubsystem::IsStageActive() const
{
    const auto* State = GetSecurityState();
    return !bTransitioningStage && State && State->State.bStageActive;
}
USPStealthPlayerStateComponent* USPGuardAlertSubsystem::RegisterPlayer(APlayerState* Player)
{
    if (!IsServer() || !IsValid(Player) || Player->GetWorld() != GetWorld() || !Player->HasAuthority()) { return nullptr; }
    const auto* Security = GetSecurityState();
    if (!Security) { return nullptr; }
    auto* State = Player->FindComponentByClass<USPStealthPlayerStateComponent>();
    if (!State)
    {
        State = NewObject<USPStealthPlayerStateComponent>(Player);
        Player->AddInstanceComponent(State);
        State->RegisterComponent();
    }
    if (State->State.StageId != Security->State.StageId)
    {
        State->State = FSPStealthIdentityState();
        State->State.StageId = Security->State.StageId;
        State->Publish();
    }
    return State;
}
void USPGuardAlertSubsystem::ResetPlayersAndGuards(const FGuid& StageId)
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (auto* Activity=It->FindComponentByClass<USPStealthActivityComponent>()) { Activity->ResetForStage(); }
        TArray<USPStealthObserverComponent*> Observers; It->GetComponents(Observers);
        for (auto* Observer:Observers) { Observer->ResetObservations(); }
    }
    for (TActorIterator<APlayerState> It(GetWorld()); It; ++It)
    {
        if (auto* State = RegisterPlayer(*It))
        {
            State->State = FSPStealthIdentityState();
            State->State.StageId = StageId;
            State->Publish();
        }
    }
    for (TActorIterator<ASPGuardCharacter> It(GetWorld()); It; ++It) { It->ResetSecurityResponse(); }
    ProcessedIds.Reset();
}
FGuid USPGuardAlertSubsystem::StartStage()
{
    if (bProcessing || bTransitioningStage) { return FGuid(); }
    auto* Security = EnsureSecurityState();
    if (!Security) { return FGuid(); }
    // 작업 취소 중 BP/네이티브 콜백이 다시 사건을 제출할 수 있다.
    // 모든 플레이어·관찰자·경비 정리가 끝날 때까지 IsStageActive에서 생산자를 막는다.
    TGuardValue<bool> Transition(bTransitioningStage, true);
    Security->State = FSPStealthAlarmState();
    Security->State.StageId = FGuid::NewGuid();
    Security->State.bStageActive = true;
    ResetPlayersAndGuards(Security->State.StageId);
    Security->Publish();
    UE_LOG(LogSPStealth, Log, TEXT("StageStart stage=%s"), *Security->State.StageId.ToString());
    return Security->State.StageId;
}
FGuid USPGuardAlertSubsystem::RestartStage() { return StartStage(); }
bool USPGuardAlertSubsystem::EndStage()
{
    if (!IsServer() || bProcessing || bTransitioningStage) { return false; }
    auto* Security = GetSecurityState();
    if (!Security) { return false; }
    TGuardValue<bool> Transition(bTransitioningStage, true);
    const FGuid StageId = Security->State.StageId;
    Security->State = FSPStealthAlarmState();
    Security->State.StageId = StageId;
    ResetPlayersAndGuards(StageId);
    Security->Publish();
    UE_LOG(LogSPStealth, Log, TEXT("StageEnd stage=%s"), *StageId.ToString());
    return true;
}
bool USPGuardAlertSubsystem::IsIdentified(FName Scope, const APlayerState* Player) const
{
    const auto* Security = GetSecurityState();
    const auto* State = IsValid(Player) && Player->GetWorld() == GetWorld()
        ? Player->FindComponentByClass<USPStealthPlayerStateComponent>() : nullptr;
    return !bTransitioningStage && Security && Security->State.bStageActive && State
        && State->State.StageId == Security->State.StageId && State->IsKnownTo(Scope);
}
FSPStealthIncidentContext USPGuardAlertSubsystem::MakeIncidentContext(FVector Location, FName Group) const
{
    FSPStealthIncidentContext Context;
    if (IsServer())
    {
        Context.IncidentId = FGuid::NewGuid();
        if (const auto* State = GetSecurityState()) { Context.StageId = State->State.StageId; }
        Context.Location = Location;
        Context.DispatchGroup = Group;
    }
    return Context;
}
FSPStealthIncidentRecord USPGuardAlertSubsystem::SubmitAnonymousIncident(ESPStealthIncident Kind,
    const FSPStealthIncidentContext& Context)
{
    return Process(Kind, Context, nullptr, NAME_None, false);
}
FSPStealthIncidentRecord USPGuardAlertSubsystem::ReportWorkNoise(const FSPStealthIncidentContext& Context)
{
    return SubmitAnonymousIncident(ESPStealthIncident::WorkNoise,Context);
}
FSPStealthIncidentRecord USPGuardAlertSubsystem::SubmitDirectIncident(ESPStealthIncident Kind,
    APlayerState* Player, FName Scope, const FSPStealthIncidentContext& Context)
{
    return Process(Kind, Context, Player, Scope, true);
}
FSPStealthIncidentRecord USPGuardAlertSubsystem::Audit(FSPStealthIncidentRecord Record)
{
    UE_LOG(LogSPStealth, Log, TEXT("Incident id=%s stage=%s kind=%s location=%s time=%.3f result=%s player=%s scope=%s dispatch=%s guards=%d firstIdentity=%d firstAlarm=%d"),
        *Record.Context.IncidentId.ToString(), *Record.Context.StageId.ToString(),
        *UEnum::GetValueAsString(Record.Kind), *Record.Context.Location.ToString(), Record.ServerTime,
        *UEnum::GetValueAsString(Record.Result), *GetNameSafe(Record.Player.Get()), *Record.IdentityScope.ToString(),
        *Record.Context.DispatchGroup.ToString(), Record.DispatchedGuards, Record.bFirstIdentification, Record.bFirstGlobalAlarm);
    if (RecentIncidents.Num() >= 128) { RecentIncidents.RemoveAt(0); }
    RecentIncidents.Add(Record);
    if (Record.Context.CrimeKind!=ESPCrimeKind::None || Record.Context.bRestrictedArea)
    { UE_LOG(LogSPStealth,Log,TEXT("Evidence incident=%s crime=%s restricted=%d"),*Record.Context.IncidentId.ToString(),*UEnum::GetValueAsString(Record.Context.CrimeKind),Record.Context.bRestrictedArea); }
    OnIncidentProcessedNative.Broadcast(Record);
    OnIncidentProcessed.Broadcast(Record);
    return Record;
}
FSPStealthIncidentRecord USPGuardAlertSubsystem::Process(ESPStealthIncident Kind,
    const FSPStealthIncidentContext& Context, APlayerState* Player, FName Scope, bool bDirectAPI)
{
    FSPStealthIncidentRecord R;
    R.Kind = Kind; R.Context = Context; R.Player = Player; R.IdentityScope = Scope;
    const auto* GS = GetWorld()->GetGameState();
    R.ServerTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    if (!IsServer()) { R.Result = ESPStealthResult::NotAuthority; return R; }
    TGuardValue<bool> Processing(bProcessing, true);
    auto* Security = GetSecurityState();
    if (bTransitioningStage || !Security || !Security->State.bStageActive) { R.Result = ESPStealthResult::InactiveStage; return Audit(R); }
    if (Context.StageId != Security->State.StageId) { R.Result = ESPStealthResult::StaleStage; return Audit(R); }
    if (!Context.IncidentId.IsValid() || Context.Location.ContainsNaN()
        || !FMath::IsFinite(Context.ResponseRadius) || Context.ResponseRadius < 0 || Context.MaxResponders < 0) { return Audit(R); }
    if (ProcessedIds.Contains(Context.IncidentId)) { R.Result = ESPStealthResult::Duplicate; return Audit(R); }
    // 사건 종류를 추가할 때는 API 허용 종류, 개인 발각, 전체 경보를 각각 검토한다.
    // 특히 위치 조사 사건을 직접 범죄 확인에 포함하면 접촉자/동료가 잘못 발각될 수 있다.
    const bool bConfirm = Kind == ESPStealthIncident::SustainedCrimeConfirmed
        || Kind == ESPStealthIncident::InstantCrimeWitnessed || Kind == ESPStealthIncident::DirectReportCompleted;
    const bool bRediscover = Kind == ESPStealthIncident::IdentifiedPlayerRediscovered;
    const bool bAnonymous = Kind == ESPStealthIncident::LaserContact || Kind == ESPStealthIncident::WorkNoise
        || Kind == ESPStealthIncident::IndirectReport || Kind == ESPStealthIncident::VictimReport
        || Kind == ESPStealthIncident::EscapeActivated;
    if ((bDirectAPI && (!bConfirm && !bRediscover)) || (!bDirectAPI && !bAnonymous)) { return Audit(R); }
    USPStealthPlayerStateComponent* Identity = nullptr;
    if (bDirectAPI)
    {
        if (Scope.IsNone() || !IsValid(Player) || Player->GetWorld() != GetWorld()) { return Audit(R); }
        Identity = RegisterPlayer(Player);
        if (!Identity) { return Audit(R); }
        if (bRediscover && !IsIdentified(Scope, Player)) { R.Result = ESPStealthResult::UnknownIdentity; return Audit(R); }
    }
    ProcessedIds.Add(Context.IncidentId); // 콜백이 같은 사건을 재제출해도 효과가 반복되지 않도록 먼저 예약한다.
    R.Result = ESPStealthResult::Applied;
    if (bConfirm)
    {
        R.bFirstIdentification = !Identity->IsIdentified();
        R.bNewIdentityScope = !Identity->IsKnownTo(Scope);
        Identity->State.KnownToScopes.AddUnique(Scope);
        if (R.bFirstIdentification) { Identity->State.FirstIdentifiedAt = R.ServerTime; }
    }
    const bool bRaiseAlarm = bConfirm || Kind == ESPStealthIncident::VictimReport || Kind == ESPStealthIncident::EscapeActivated;
    if (bRaiseAlarm && !Security->State.bGlobalAlarm)
    {
        R.bFirstGlobalAlarm = true;
        Security->State.bGlobalAlarm = true;
        Security->State.AlarmServerTime = R.ServerTime;
        Security->State.AlarmIncidentId = Context.IncidentId;
    }
    ASPPlayerCharacter* Pawn = Player ? Cast<ASPPlayerCharacter>(Player->GetPawn()) : nullptr;
    // 누가 신원을 아는지와 누가 현장에 출동할지는 별개다.
    // 사건 당시 위치에서 거리/그룹으로 후보를 정하고, 지시를 수락한 경비만 정원에 센다.
    TArray<ASPGuardCharacter*> Candidates;
    for (TActorIterator<ASPGuardCharacter> It(GetWorld()); !Context.DispatchGroup.IsNone() && It; ++It)
    {
        if (IsValid(*It) && It->AlertGroup == Context.DispatchGroup && Context.ResponseRadius > 0
            && FVector::DistSquared(It->GetActorLocation(), Context.Location) <= FMath::Square(Context.ResponseRadius))
        { Candidates.Add(*It); }
    }
    Candidates.Sort([&Context](const ASPGuardCharacter& A, const ASPGuardCharacter& B)
    {
        const double DA = FVector::DistSquared(A.GetActorLocation(), Context.Location);
        const double DB = FVector::DistSquared(B.GetActorLocation(), Context.Location);
        return DA == DB ? A.GetPathName() < B.GetPathName() : DA < DB;
    });
    for (ASPGuardCharacter* Guard : Candidates)
    {
        if (R.DispatchedGuards >= Context.MaxResponders) { break; }
        if (Pawn && Guard->IdentityScope == Scope)
        {
            if (Guard->ReceiveSighting(Pawn, Context.Location)) { ++R.DispatchedGuards; }
        }
        else if (Guard->ReceiveAnonymousIncident(Context)) { ++R.DispatchedGuards; }
    }
    if (R.bNewIdentityScope) { Identity->Publish(); }
    if (R.bFirstGlobalAlarm) { Security->Publish(); }
    // 상태 원본과 중복 ID를 먼저 확정한 뒤 최초 효과를 알린다.
    // UI의 OnRep/상태 갱신에서 같은 최초 효과를 다시 실행하지 않는다.
    if (R.bFirstIdentification) { OnFirstPlayerIdentifiedNative.Broadcast(R); OnFirstPlayerIdentified.Broadcast(R); }
    if (R.bFirstGlobalAlarm) { OnFirstGlobalAlarmNative.Broadcast(R); OnFirstGlobalAlarm.Broadcast(R); }
    return Audit(R);
}
void USPGuardAlertSubsystem::ReportSighting(ASPGuardCharacter* Witness, ASPPlayerCharacter* Player, const FVector& Location)
{
    // 기존 경비 호출부를 위한 연결 함수다. 호출 전 실제 시야/범죄 확인은 경비가 끝내야 한다.
    if (!IsServer() || !IsValid(Witness) || Witness->GetWorld() != GetWorld() || !Witness->HasAuthority()
        || !IsValid(Player) || !Player->IsPlayerControlled() || Player->GetWorld() != GetWorld()) { return; }
    auto Context = MakeIncidentContext(Location, Witness->AlertGroup);
    Context.ResponseRadius = Witness->SightingResponseRadius;
    Context.MaxResponders = Witness->SightingMaxResponders;
    SubmitDirectIncident(IsIdentified(Witness->IdentityScope, Player->GetPlayerState())
        ? ESPStealthIncident::IdentifiedPlayerRediscovered : ESPStealthIncident::SustainedCrimeConfirmed,
        Player->GetPlayerState(), Witness->IdentityScope, Context);
}
