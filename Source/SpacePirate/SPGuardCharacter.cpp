// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 다운된 플레이어를 시야·청각·목격 전달·추적 대상에서 제외한다.

#include "SPGuardCharacter.h"
#include "SPGuardAlertSubsystem.h"
#include "SPGuardPatrolRoute.h"
#include "SPRestrictedArea.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSPGuardResponse, Log, All);

ASPGuardCharacter::ASPGuardCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 240, 0);
    GetCharacterMovement()->bUseRVOAvoidance = true;
    GetMesh()->SetOwnerNoSee(false);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);
    FirstPersonCamera->SetAutoActivate(false);
    Observer = CreateDefaultSubobject<USPStealthObserverComponent>(TEXT("Observer"));
    Observer->SetupAttachment(GetCapsuleComponent());
    Observer->SetRelativeLocation(FVector(0,0,64));
    Observer->bAutoObserve = false;
    AlertIndicator = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AlertIndicator"));
    AlertIndicator->SetupAttachment(GetCapsuleComponent());
    AlertIndicator->SetRelativeLocation(FVector(0, 0, 135));
    AlertIndicator->SetHorizontalAlignment(EHTA_Center);
    AlertIndicator->SetVerticalAlignment(EVRTA_TextCenter);
    AlertIndicator->SetWorldSize(24);
    AlertIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AlertIndicator->SetVisibility(false);
}

void ASPGuardCharacter::BeginPlay()
{
    Super::BeginPlay();
    Observer->RefreshSettings.BindUObject(this,&ThisClass::ConfigureObserver);
    Observer->OnWitnessConfirmedNative.AddUObject(this,&ThisClass::HandleWitness);
    ConfigureObserver();
    PatrolIndex = PatrolRoute && !PatrolRoute->Points.IsEmpty()
        ? FMath::Clamp(StartPointIndex, 0, PatrolRoute->Points.Num() - 1) : 0;
    UpdateFeedback();
}

void ASPGuardCharacter::OnMovementModeChanged(EMovementMode PreviousMode, uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(PreviousMode, PreviousCustomMode);
    bUseControllerRotationYaw = false;
}

void ASPGuardCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPGuardCharacter, GuardState);
    DOREPLIFETIME(ASPGuardCharacter, TargetPlayer);
    DOREPLIFETIME(ASPGuardCharacter, LastSeenLocation);
    DOREPLIFETIME(ASPGuardCharacter, SuspicionProgress);
    DOREPLIFETIME(ASPGuardCharacter, HeardLocation);
    DOREPLIFETIME(ASPGuardCharacter, bInvestigatingAnonymousIncident);
    DOREPLIFETIME(ASPGuardCharacter, InvestigationLocation);
    DOREPLIFETIME(ASPGuardCharacter, InvestigationIncidentId);
    DOREPLIFETIME(ASPGuardCharacter, LastInvestigation);
    DOREPLIFETIME(ASPGuardCharacter, LastMoveFailure);
    DOREPLIFETIME(ASPGuardCharacter, MoveRequestCount);
}

void ASPGuardCharacter::ConfigureObserver()
{
    Observer->Sight.Distance=SightDistance;
    Observer->Sight.HorizontalAngle=SightAngle;
    Observer->Sight.VerticalAngle=VerticalSightAngle;
    Observer->Sight.ConfirmationSeconds=ConfirmSightTime;
}
FVector ASPGuardCharacter::GetSightOrigin() const { return Observer->GetComponentLocation(); }
bool ASPGuardCharacter::CanSeePlayer(const ASPPlayerCharacter* Player) const { return Observer->CanSeePlayer(Player); }
void ASPGuardCharacter::HandleWitness(const FSPStealthWitness& Witness)
{
    auto* Player=Cast<ASPPlayerCharacter>(Witness.Player.Get());
    auto* S=GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    if (!HasAuthority() || !IsValid(Player) || Player->IsDowned() || !S) { return; }
    auto Context=S->MakeIncidentContext(Witness.Location,AlertGroup);
    Context.IncidentId=Witness.WitnessId;
    Context.CrimeKind=Witness.Crime;
    Context.bRestrictedArea=Witness.bRestricted;
    Context.ResponseRadius=SightingResponseRadius;
    Context.MaxResponders=SightingMaxResponders;
    S->SubmitDirectIncident(Witness.bInstant?ESPStealthIncident::InstantCrimeWitnessed:ESPStealthIncident::SustainedCrimeConfirmed,
        Player->GetPlayerState(),IdentityScope,Context);
    if (S->IsIdentified(IdentityScope,Player->GetPlayerState())) { ReceiveSighting(Player,Witness.Location); }
    NextSignalTime=GetWorld()->GetTimeSeconds()+0.4f;
}

bool ASPGuardCharacter::CanHearPlayer(const ASPPlayerCharacter* Player) const
{
    if (!bHearFootsteps || !IsValid(Player) || Player == this || !Player->IsPlayerControlled() || Player->IsDowned()
        || Player->bIsCrouched || !Player->GetCharacterMovement()->IsMovingOnGround()
        || Player->GetVelocity().SizeSquared2D() < FMath::Square(MinimumFootstepSpeed)
        || FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) > FMath::Square(HearingDistance))
    {
        return false;
    }
    const USPGuardAlertSubsystem* Alerts = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    return (ASPRestrictedArea::FindAtLocation(this,Player->GetActorLocation())!=nullptr)
        || (Alerts && Alerts->IsIdentified(IdentityScope, Player->GetPlayerState()));
}

void ASPGuardCharacter::UpdateHearing(float DeltaSeconds)
{
    // 직접 목격/확정한 대상이 우선이다. 소리는 신원 확정이나 동료 호출을 하지 않는다.
    if (TargetPlayer || SuspicionProgress > 0 || bInvestigatingAnonymousIncident)
    {
        ListeningRemaining = 0;
        HeardPlayer.Reset();
        return;
    }
    ListeningRemaining = FMath::Max(0.0f, ListeningRemaining - DeltaSeconds);
    FootstepSampleRemaining -= DeltaSeconds;
    if (bHearFootsteps && FootstepSampleRemaining <= 0)
    {
        FootstepSampleRemaining = 0.25f;
        ASPPlayerCharacter* Source = CanHearPlayer(HeardPlayer.Get()) ? HeardPlayer.Get() : nullptr;
        float ClosestDistance = UE_BIG_NUMBER;
        if (!Source)
        {
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            {
                const APlayerController* PC = It->Get();
                ASPPlayerCharacter* Player = PC ? Cast<ASPPlayerCharacter>(PC->GetPawn()) : nullptr;
                if (!CanHearPlayer(Player)) { continue; }
                const float Distance = FVector::DistSquared(GetActorLocation(), Player->GetActorLocation());
                if (Distance < ClosestDistance) { Source = Player; ClosestDistance = Distance; }
            }
        }
        if (Source)
        {
            HeardPlayer = Source;
            HeardLocation = Source->GetActorLocation();
            ListeningRemaining = ListenTime;
        }
    }
    if (bHearFootsteps && ListeningRemaining > 0)
    {
        GuardState = ESPGuardState::Listening;
    }
    else { HeardPlayer.Reset(); }
}

bool ASPGuardCharacter::ReceiveSighting(ASPPlayerCharacter* Player, const FVector& Location)
{
    if (!HasAuthority() || !IsValid(Player) || !Player->IsPlayerControlled() || Player->IsDowned()
        || Location.ContainsNaN() || ResponseCooldownRemaining > 0) { return false; }
    // 이미 눈앞에서 쫓는 다른 범인을 무선 신호만으로 바꾸지 않는다.
    if (IsValid(TargetPlayer) && TargetPlayer != Player && CanSeePlayer(TargetPlayer)) { return false; }
    if (bInvestigatingAnonymousIncident) { FinishInvestigation(ESPGuardInvestigationResult::InterruptedBySighting); }
    if (TargetPlayer != Player || GuardState == ESPGuardState::Searching) { ResetMovementTask(); }
    TargetPlayer = Player;
    bInvestigatingAnonymousIncident = false;
    ListeningRemaining = 0;
    HeardPlayer.Reset();
    LastSeenLocation = Location;
    LastSightingTime = GetWorld()->GetTimeSeconds();
    GuardState = ESPGuardState::Pursuing;
    SearchRemaining = SearchTime;
    ForceNetUpdate();
    return true;
}

void ASPGuardCharacter::UpdateSight(float DeltaSeconds)
{
    USPGuardAlertSubsystem* Alerts=GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    if (!Alerts || !Alerts->IsStageActive()) { return; }
    ConfigureObserver();
    bTargetVisible=false;
    SuspicionProgress=0;
    for (FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
    {
        APlayerController* PC=It->Get();
        ASPPlayerCharacter* Player=PC?Cast<ASPPlayerCharacter>(PC->GetPawn()):nullptr;
        const FSPStealthObservation Observation=Observer->SamplePlayer(Player,DeltaSeconds);
        if (!Player || !Observation.bVisible) { continue; }
        const bool bKnown=Alerts->IsIdentified(IdentityScope,Player->GetPlayerState());
        if (bKnown || Observation.bConfirmed)
        {
            if ((!bKnown || GetWorld()->GetTimeSeconds()>=NextSignalTime) && IsValid(Player->GetPlayerState()))
            {
                Alerts->ReportSighting(this,Player,Player->GetActorLocation());
                NextSignalTime=GetWorld()->GetTimeSeconds()+0.4f;
            }
            if (!IsValid(TargetPlayer) && Alerts->IsIdentified(IdentityScope,Player->GetPlayerState()))
            { ReceiveSighting(Player,Player->GetActorLocation()); }
            if (TargetPlayer==Player)
            {
                bTargetVisible=true;
                LastSeenLocation=Player->GetActorLocation();
                LastSightingTime=GetWorld()->GetTimeSeconds();
                GuardState=ESPGuardState::Pursuing;
            }
        }
        else { SuspicionProgress=FMath::Max(SuspicionProgress,Observation.Progress); }
    }
    if (!TargetPlayer)
    {
        GuardState=SuspicionProgress>0?ESPGuardState::Suspicious
            :bInvestigatingAnonymousIncident?(bAtInvestigationLocation?ESPGuardState::SceneSearching:ESPGuardState::Investigating)
            :ESPGuardState::Patrol;
    }
}

bool ASPGuardCharacter::HasReached(const FVector& Location, float Radius) const
{
    // Foot locations and pawn-center sightings are both valid; a different floor is not arrival.
    return FVector::Dist2D(GetActorLocation(), Location) <= Radius
        && FMath::Abs(GetActorLocation().Z - Location.Z) <= GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 60;
}

void ASPGuardCharacter::PauseMovement()
{
    if (AAIController* AI = Cast<AAIController>(Controller)) { AI->StopMovement(); }
    bMoveActive = false;
}

void ASPGuardCharacter::ResetMovementTask()
{
    PauseMovement();
    RequestedDestination = FVector(UE_BIG_NUMBER);
    MoveRetryRemaining = MoveElapsed = NoProgressElapsed = 0;
    MoveFailures = MoveRequestCount = 0;
    bMoveExhausted = false;
    LastMoveFailure = ESPGuardMoveFailure::None;
}

ASPGuardCharacter::EMoveProgress ASPGuardCharacter::FailMove(ESPGuardMoveFailure Failure)
{
    PauseMovement();
    LastMoveFailure = Failure;
    ++MoveFailures;
    bMoveExhausted = MoveFailures > FMath::Clamp(MaxMoveRetries, 0, 10) || Failure == ESPGuardMoveFailure::TimedOut;
    MoveRetryRemaining = FMath::Max(MoveRetryInterval, 0.1f);
    UE_LOG(LogSPGuardResponse, Log, TEXT("MoveFailure guard=%s incident=%s destination=%s cause=%s failures=%d exhausted=%d"),
        *GetName(), *(bInvestigatingAnonymousIncident ? InvestigationIncidentId : FGuid()).ToString(), *RequestedDestination.ToString(),
        *UEnum::GetValueAsString(Failure), MoveFailures, bMoveExhausted);
    ForceNetUpdate();
    return bMoveExhausted ? EMoveProgress::Failed : EMoveProgress::Moving;
}

ASPGuardCharacter::EMoveProgress ASPGuardCharacter::MoveToLocation(const FVector& Location, float AcceptanceRadius, float DeltaSeconds)
{
    if (HasReached(Location, AcceptanceRadius)) { PauseMovement(); return EMoveProgress::Arrived; }
    if (bMoveExhausted) { return EMoveProgress::Failed; }
    MoveElapsed += DeltaSeconds;
    if (MoveElapsed >= FMath::Max(MoveTimeout, 1.0f)) { return FailMove(ESPGuardMoveFailure::TimedOut); }
    MoveRetryRemaining -= DeltaSeconds;
    AAIController* AI = Cast<AAIController>(Controller);
    // 새 목격으로 목적지가 바뀌어도 실패 횟수·전체 이동 제한 시간은 유지한다. 계속 바뀌는 목표로 무한 재시도하지 않는다.
    const bool bChanged = !RequestedDestination.Equals(Location, 50);
    if (bMoveActive && !bChanged)
    {
        if (FVector::DistSquared(ProgressLocation, GetActorLocation()) >= FMath::Square(25.0f))
        { ProgressLocation = GetActorLocation(); NoProgressElapsed = 0; }
        else { NoProgressElapsed += DeltaSeconds; }
        if (NoProgressElapsed >= FMath::Max(StuckTimeout, 0.5f)) { return FailMove(ESPGuardMoveFailure::Stalled); }
        if (!AI || AI->GetMoveStatus() == EPathFollowingStatus::Idle) { return FailMove(ESPGuardMoveFailure::Blocked); }
        return EMoveProgress::Moving;
    }
    if (MoveRetryRemaining > 0) { return EMoveProgress::Moving; }
    PauseMovement();
    // 마지막으로 확인된 좌표를 사용한다. 숨은 Pawn에 MoveToActor를 걸면 실제 위치를 계속 알아내게 된다.
    RequestedDestination = Location;
    ++MoveRequestCount;
    if (!AI) { return FailMove(ESPGuardMoveFailure::NoController); }
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const ANavigationData* Data = Nav ? Nav->GetNavDataForProps(GetNavAgentPropertiesRef(), GetActorLocation()) : nullptr;
    if (!Data) { return FailMove(ESPGuardMoveFailure::NoNavigation); }
    FNavLocation Projected;
    // 투영 범위를 제한해 도달 불가인 위층 목적지가 아래층 NavMesh로 대체되지 않게 한다.
    if (!Nav->ProjectPointToNavigation(Location, Projected, FVector(80, 80, 160), Data))
    { return FailMove(ESPGuardMoveFailure::Unreachable); }
    FPathFindingQuery Query(this, *Data, GetNavAgentLocation(), Projected.Location);
    Query.SetAllowPartialPaths(false);
    const FPathFindingResult Path = Nav->FindPathSync(GetNavAgentPropertiesRef(), Query);
    if (!Path.IsSuccessful() || !Path.Path.IsValid() || Path.Path->IsPartial())
    { return FailMove(ESPGuardMoveFailure::Unreachable); }
    const EPathFollowingRequestResult::Type Request = AI->MoveToLocation(Projected.Location, AcceptanceRadius,
        false, true, false, false, nullptr, false);
    if (Request == EPathFollowingRequestResult::Failed) { return FailMove(ESPGuardMoveFailure::Blocked); }
    if (Request == EPathFollowingRequestResult::AlreadyAtGoal)
    {
        // NavMesh 투영점만으로 도착 처리하지 않고 원래 사건 위치까지의 높이 차이도 확인한다.
        return HasReached(Location, AcceptanceRadius + 20) ? EMoveProgress::Arrived : FailMove(ESPGuardMoveFailure::Unreachable);
    }
    bMoveActive = true;
    ProgressLocation = GetActorLocation();
    NoProgressElapsed = 0;
    MoveRetryRemaining = 0.35f;
    ForceNetUpdate();
    return EMoveProgress::Moving;
}

void ASPGuardCharacter::FinishInvestigation(ESPGuardInvestigationResult Result)
{
    if (!bInvestigatingAnonymousIncident) { return; }
    LastInvestigation.IncidentId = InvestigationIncidentId;
    LastInvestigation.Location = InvestigationLocation;
    LastInvestigation.Result = Result;
    LastInvestigation.Failure = Result == ESPGuardInvestigationResult::Failed ? LastMoveFailure : ESPGuardMoveFailure::None;
    LastInvestigation.MoveRequests = MoveRequestCount;
    const auto* GS = GetWorld()->GetGameState();
    LastInvestigation.ServerTime = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    UE_LOG(LogSPGuardResponse, Log, TEXT("Investigation guard=%s incident=%s location=%s time=%.3f result=%s failure=%s requests=%d"),
        *GetName(), *InvestigationIncidentId.ToString(), *InvestigationLocation.ToString(), LastInvestigation.ServerTime,
        *UEnum::GetValueAsString(Result), *UEnum::GetValueAsString(LastInvestigation.Failure), MoveRequestCount);
    bInvestigatingAnonymousIncident = false;
    bAtInvestigationLocation = false;
    ForceNetUpdate();
}

void ASPGuardCharacter::ReturnToPatrol()
{
    // 이 경비의 행동만 종료한다. PlayerState의 스테이지 발각 기록과 전체 경보는 유지한다.
    TargetPlayer = nullptr;
    bInvestigatingAnonymousIncident = false;
    bAtInvestigationLocation = false;
    ListeningRemaining = 0;
    HeardPlayer.Reset();
    GuardState = ESPGuardState::Patrol;
    ResetMovementTask();
    // Returning to patrol never clears PlayerState identity knowledge or the completed receipt.
    ForceNetUpdate();
}

bool ASPGuardCharacter::ReceiveAnonymousIncident(const FSPStealthIncidentContext& Incident)
{
    // 확인된 대상을 추적 중이면 익명 호출을 거절한다. 가려졌더라도 마지막 목격 위치 수색이 우선이다.
    if (!HasAuthority() || !Incident.IncidentId.IsValid() || Incident.Location.ContainsNaN() || IsValid(TargetPlayer)) { return false; }
    // 완료한 사건의 재호출도 병합한다. Subsystem 역시 해당 스테이지의 처리 ID를 보관한다.
    if (InvestigationIncidentId == Incident.IncidentId) { return true; }
    if (ResponseCooldownRemaining > 0) { return false; }
    FinishInvestigation(ESPGuardInvestigationResult::Superseded);
    ResetMovementTask();
    InvestigationIncidentId = Incident.IncidentId;
    bInvestigatingAnonymousIncident = true;
    bAtInvestigationLocation = false;
    InvestigationLocation = Incident.Location;
    SearchRemaining = FMath::Max(SearchTime, 0.1f);
    ListeningRemaining = 0;
    HeardPlayer.Reset();
    GuardState = ESPGuardState::Investigating;
    ForceNetUpdate();
    return true;
}

void ASPGuardCharacter::ResetSecurityResponse()
{
    if (!HasAuthority()) { return; }
    FinishInvestigation(ESPGuardInvestigationResult::StageReset);
    ReturnToPatrol();
    LastInvestigation = FSPGuardInvestigationReceipt();
    InvestigationIncidentId.Invalidate();
    Observer->ResetObservations();
    SuspicionProgress = 0;
    LastSeenLocation = HeardLocation = InvestigationLocation = FVector::ZeroVector;
    bTargetVisible = false;
    NextSignalTime = LastSightingTime = FootstepSampleRemaining = 0;
    SearchRemaining = PatrolWaitRemaining = ResponseCooldownRemaining = PatrolFailureWait = 0;
    PatrolDirection = 1;
    PatrolIndex = PatrolRoute && !PatrolRoute->Points.IsEmpty()
        ? FMath::Clamp(StartPointIndex, 0, PatrolRoute->Points.Num() - 1) : 0;
}

void ASPGuardCharacter::AdvancePatrolPoint()
{
    PatrolWaitRemaining = 0;
    ResetMovementTask();
    if (!PatrolRoute || PatrolRoute->Points.IsEmpty()) { return; }
    if (PatrolRoute->bLoop) { PatrolIndex = (PatrolIndex + 1) % PatrolRoute->Points.Num(); }
    else if (PatrolRoute->Points.Num() > 1)
    {
        if (PatrolIndex + PatrolDirection >= PatrolRoute->Points.Num() || PatrolIndex + PatrolDirection < 0)
        { PatrolDirection *= -1; }
        PatrolIndex += PatrolDirection;
    }
}

void ASPGuardCharacter::UpdateBehavior(float DeltaSeconds)
{
    // 우선순위: 확인된 대상 추적 → 목격 확인/소리 듣기 → 익명 현장 조사 → 순찰.
    // UpdateSight가 먼저 실행되어 실제 범죄를 확인하면 익명 조사 중에도 대상 추적으로 전환할 수 있다.
    GetCharacterMovement()->MaxWalkSpeed = TargetPlayer ? ChaseSpeed : PatrolSpeed;
    if (TargetPlayer)
    {
        if (!IsValid(TargetPlayer) || !TargetPlayer->IsPlayerControlled() || TargetPlayer->IsDowned()) { ReturnToPatrol(); return; }
        if (bTargetVisible)
        {
            GuardState = ESPGuardState::Pursuing;
            if (HasReached(LastSeenLocation, 115))
            {
                ResetMovementTask();
                SetActorRotation(FRotator(0, (LastSeenLocation - GetActorLocation()).Rotation().Yaw, 0));
            }
            else if (MoveToLocation(LastSeenLocation, 90, DeltaSeconds) == EMoveProgress::Failed)
            { ResponseCooldownRemaining = FailedResponseCooldown; ReturnToPatrol(); }
            return;
        }
        if (GuardState != ESPGuardState::Searching)
        {
            GuardState = ESPGuardState::MovingToLastSeen;
            if (GetWorld()->GetTimeSeconds() - LastSightingTime >= LostTargetTimeout)
            { FailMove(ESPGuardMoveFailure::TimedOut); ReturnToPatrol(); return; }
            const auto Progress = MoveToLocation(LastSeenLocation, 80, DeltaSeconds);
            if (Progress == EMoveProgress::Failed)
            { ResponseCooldownRemaining = FailedResponseCooldown; ReturnToPatrol(); return; }
            if (Progress != EMoveProgress::Arrived) { return; }
            GuardState = ESPGuardState::Searching;
            SearchRemaining = FMath::Max(SearchTime, 0.1f);
            ForceNetUpdate();
        }
        SearchRemaining -= DeltaSeconds;
        AddActorWorldRotation(FRotator(0, 50 * DeltaSeconds, 0));
        if (SearchRemaining <= 0) { ReturnToPatrol(); }
        return;
    }
    if (GuardState == ESPGuardState::Suspicious || GuardState == ESPGuardState::Listening)
    {
        PauseMovement();
        if (GuardState == ESPGuardState::Listening)
        {
            GetCharacterMovement()->StopMovementImmediately();
            const FVector ToSound = HeardLocation - GetActorLocation();
            if (!ToSound.IsNearlyZero())
            { SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), FRotator(0, ToSound.Rotation().Yaw, 0), DeltaSeconds, HearingTurnSpeed)); }
        }
        return;
    }
    if (bInvestigatingAnonymousIncident)
    {
        // TargetPlayer 없이 사건 좌표만으로 이동·수색을 끝낸다. 수색 시간은 현장에 도착한 뒤부터 소모한다.
        if (!bAtInvestigationLocation)
        {
            GuardState = ESPGuardState::Investigating;
            const auto Progress = MoveToLocation(InvestigationLocation, 80, DeltaSeconds);
            if (Progress == EMoveProgress::Failed)
            {
                FinishInvestigation(ESPGuardInvestigationResult::Failed);
                ResponseCooldownRemaining = FailedResponseCooldown;
                ReturnToPatrol();
                return;
            }
            if (Progress != EMoveProgress::Arrived) { return; }
            bAtInvestigationLocation = true;
            SearchRemaining = FMath::Max(SearchTime, 0.1f);
            GuardState = ESPGuardState::SceneSearching;
            ForceNetUpdate();
        }
        SearchRemaining -= DeltaSeconds;
        AddActorWorldRotation(FRotator(0, 50 * DeltaSeconds, 0));
        if (SearchRemaining <= 0)
        { FinishInvestigation(ESPGuardInvestigationResult::Completed); ReturnToPatrol(); }
        return;
    }
    GuardState = ESPGuardState::Patrol;
    if (!PatrolRoute || PatrolRoute->Points.IsEmpty()) { PauseMovement(); return; }
    if (PatrolFailureWait > 0) { PatrolFailureWait -= DeltaSeconds; return; }
    PatrolIndex = FMath::Clamp(PatrolIndex, 0, PatrolRoute->Points.Num() - 1);
    const auto Progress = MoveToLocation(PatrolRoute->GetPatrolLocation(PatrolIndex), 60, DeltaSeconds);
    if (Progress == EMoveProgress::Arrived)
    {
        PatrolWaitRemaining += DeltaSeconds;
        if (PatrolWaitRemaining >= PatrolWaitTime) { AdvancePatrolPoint(); }
    }
    else if (Progress == EMoveProgress::Failed)
    {
        // Broken patrol points are skipped with a cooldown, including single-point routes.
        AdvancePatrolPoint();
        PatrolFailureWait = FMath::Max(FailedResponseCooldown, 0.1f);
    }
}

void ASPGuardCharacter::UpdateFeedback()
{
    if (GetNetMode() == NM_DedicatedServer) { return; }
    const TCHAR* Text = TEXT("순찰 중");
    FColor Color(50, 220, 140);
    switch (GuardState)
    {
    case ESPGuardState::Suspicious: Text = TEXT("범죄 확인 중"); Color = FColor::Yellow; break;
    case ESPGuardState::Listening: Text = TEXT("소리 확인 중"); Color = FColor::Yellow; break;
    case ESPGuardState::Investigating: Text = TEXT("사건 위치로 이동"); Color = FColor::Cyan; break;
    case ESPGuardState::SceneSearching: Text = TEXT("현장 수색 중"); Color = FColor(100, 180, 255); break;
    case ESPGuardState::Pursuing: Text = TEXT("발각자 추격 중"); Color = FColor::Red; break;
    case ESPGuardState::MovingToLastSeen: Text = TEXT("마지막 목격 위치로 이동"); Color = FColor(255, 150, 50); break;
    case ESPGuardState::Searching: Text = TEXT("마지막 목격 위치 수색"); Color = FColor(255, 150, 50); break;
    default: break;
    }
    AlertIndicator->SetVisibility(bShowStateIndicator);
    AlertIndicator->SetText(FText::FromString(Text));
    AlertIndicator->SetTextRenderColor(Color);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (PC && PC->IsLocalController())
        {
            FVector Eye; FRotator Rotation;
            PC->GetPlayerViewPoint(Eye, Rotation);
            AlertIndicator->SetWorldRotation((Eye - AlertIndicator->GetComponentLocation()).Rotation());
            break;
        }
    }
}

void ASPGuardCharacter::DrawVision() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!bShowVision || GetNetMode() == NM_DedicatedServer) { return; }
    const FVector Eye = GetSightOrigin();
    const FVector Ground = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 8);
    const FColor Color = GuardState == ESPGuardState::Pursuing ? FColor::Red
        : GuardState == ESPGuardState::Patrol ? FColor(50, 220, 140) : FColor::Yellow;
    FVector Previous = Ground;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardVisionDebug), false, this);
    constexpr int32 Segments = 32;
    for (int32 I = 0; I <= Segments; ++I)
    {
        const float Angle = -SightAngle * 0.5f + SightAngle * I / Segments;
        const FVector Direction = GetActorForwardVector().RotateAngleAxis(Angle, FVector::UpVector);
        FVector End = Eye + Direction * SightDistance;
        FHitResult Hit;
        if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Params)) { End = Hit.ImpactPoint; }
        const FVector FloorEnd(End.X, End.Y, Ground.Z);
        if (I > 0) { DrawDebugLine(GetWorld(), Previous, FloorEnd, Color, false, -1, 0, 3); }
        if (I % 4 == 0) { DrawDebugLine(GetWorld(), Ground, FloorEnd, Color, false, -1, 0, 1.5f); }
        if (I == 0 || I == Segments) { DrawDebugLine(GetWorld(), Eye, End, Color, false, -1, 0, 2); }
        Previous = FloorEnd;
    }
#endif
}

void ASPGuardCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (HasAuthority())
    {
        ResponseCooldownRemaining = FMath::Max(0.0f, ResponseCooldownRemaining - DeltaSeconds);
        const auto* Security = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
        if (Security && Security->IsStageActive())
        {
            UpdateSight(DeltaSeconds);
            UpdateHearing(DeltaSeconds);
        }
        UpdateBehavior(DeltaSeconds);
    }
    UpdateFeedback();
    DrawVision();
}
