#include "SPGuardCharacter.h"
#include "SPGuardAlertSubsystem.h"
#include "SPGuardPatrolRoute.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

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
    AlertIndicator = CreateDefaultSubobject<UTextRenderComponent>(TEXT("AlertIndicator"));
    AlertIndicator->SetupAttachment(GetCapsuleComponent());
    AlertIndicator->SetRelativeLocation(FVector(0, 0, 135));
    AlertIndicator->SetHorizontalAlignment(EHTA_Center);
    AlertIndicator->SetVerticalAlignment(EVRTA_TextCenter);
    AlertIndicator->SetWorldSize(55);
    AlertIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AlertIndicator->SetVisibility(false);
}

void ASPGuardCharacter::BeginPlay()
{
    Super::BeginPlay();
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
}

FVector ASPGuardCharacter::GetSightOrigin() const
{
    return GetActorLocation() + FVector(0, 0, 64);
}

bool ASPGuardCharacter::CanSeePlayer(const ASPPlayerCharacter* Player) const
{
    if (!IsValid(Player) || Player == this || !Player->IsPlayerControlled()) { return false; }
    const FVector Eye = GetSightOrigin();
    const float Height = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    // 서 있는 자세/앉기의 실제 캡슐 높이에 맞춰 상체와 몸통을 검사한다.
    const FVector Samples[] = {Player->GetActorLocation() + FVector(0, 0, Height * 0.65f), Player->GetActorLocation()};
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GuardSight), false, this);
    Params.AddIgnoredActor(Player);
    const float MinDot = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(SightAngle, 1.0f, 179.0f) * 0.5f));
    for (const FVector& Sample : Samples)
    {
        const FVector Delta = Sample - Eye;
        if (Delta.SizeSquared() > FMath::Square(SightDistance)) { continue; }
        if (FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), Delta.GetSafeNormal2D()) < MinDot) { continue; }
        if (FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Delta.Z, Delta.Size2D()))) > VerticalSightAngle * 0.5f) { continue; }
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Sample, ECC_Visibility, Params)) { return true; }
    }
    return false;
}

bool ASPGuardCharacter::CanHearPlayer(const ASPPlayerCharacter* Player) const
{
    if (!bHearFootsteps || !IsValid(Player) || Player == this || !Player->IsPlayerControlled()
        || Player->bIsCrouched || !Player->GetCharacterMovement()->IsMovingOnGround()
        || Player->GetVelocity().SizeSquared2D() < FMath::Square(MinimumFootstepSpeed)
        || FVector::DistSquared(GetActorLocation(), Player->GetActorLocation()) > FMath::Square(HearingDistance))
    {
        return false;
    }
    const USPGuardAlertSubsystem* Alerts = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    return (PatrolRoute && PatrolRoute->ContainsLocation(Player->GetActorLocation()))
        || (Alerts && Alerts->IsIdentified(AlertGroup, Player->GetPlayerState()));
}

void ASPGuardCharacter::UpdateHearing(float DeltaSeconds)
{
    // 직접 목격/확정한 대상이 우선이다. 소리는 신원 확정이나 동료 호출을 하지 않는다.
    if (TargetPlayer || SuspicionProgress > 0)
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

void ASPGuardCharacter::ReceiveSighting(ASPPlayerCharacter* Player, const FVector& Location)
{
    if (!HasAuthority() || !IsValid(Player) || !Player->IsPlayerControlled()) { return; }
    // 이미 눈앞에서 쫓는 다른 범인을 무선 신호만으로 바꾸지 않는다.
    if (IsValid(TargetPlayer) && TargetPlayer != Player && CanSeePlayer(TargetPlayer)) { return; }
    TargetPlayer = Player;
    ListeningRemaining = 0;
    HeardPlayer.Reset();
    LastSeenLocation = Location;
    LastSightingTime = GetWorld()->GetTimeSeconds();
    GuardState = ESPGuardState::Pursuing;
    SearchRemaining = SearchTime;
    ForceNetUpdate();
}

void ASPGuardCharacter::UpdateSight(float DeltaSeconds)
{
    USPGuardAlertSubsystem* Alerts = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    if (!Alerts) { return; }
    bTargetVisible = false;
    SuspicionProgress = 0;
    TSet<TWeakObjectPtr<ASPPlayerCharacter>> VisibleCandidates;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        ASPPlayerCharacter* Player = PC ? Cast<ASPPlayerCharacter>(PC->GetPawn()) : nullptr;
        if (!Player || !CanSeePlayer(Player)) { continue; }
        const bool bKnown = Alerts->IsIdentified(AlertGroup, Player->GetPlayerState());
        const bool bTrespassing = PatrolRoute && PatrolRoute->ContainsLocation(Player->GetActorLocation());
        if (!bKnown && !bTrespassing) { continue; }
        VisibleCandidates.Add(Player);
        float& Time = ConfirmationTimes.FindOrAdd(Player);
        Time += DeltaSeconds;
        if (bKnown || Time >= ConfirmSightTime)
        {
            // 발각은 플레이어별로 판정한다. 다른 미발각 플레이어의 타이머에는 영향이 없다.
            if (!bKnown || GetWorld()->GetTimeSeconds() >= NextSignalTime)
            {
                Alerts->ReportSighting(this, Player, Player->GetActorLocation());
                NextSignalTime = GetWorld()->GetTimeSeconds() + 0.4f;
            }
            if (!IsValid(TargetPlayer)) { ReceiveSighting(Player, Player->GetActorLocation()); }
            if (TargetPlayer == Player)
            {
                bTargetVisible = true;
                LastSeenLocation = Player->GetActorLocation();
                LastSightingTime = GetWorld()->GetTimeSeconds();
                GuardState = ESPGuardState::Pursuing;
            }
        }
        else
        {
            SuspicionProgress = FMath::Max(SuspicionProgress, Time / FMath::Max(ConfirmSightTime, 0.001f));
        }
    }
    for (auto It = ConfirmationTimes.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || !VisibleCandidates.Contains(It.Key())) { It.RemoveCurrent(); }
    }
    if (!TargetPlayer)
    {
        GuardState = SuspicionProgress > 0 ? ESPGuardState::Suspicious : ESPGuardState::Patrol;
    }
}

void ASPGuardCharacter::MoveToLocation(const FVector& Location, float AcceptanceRadius)
{
    AAIController* AI = Cast<AAIController>(Controller);
    if (!AI) { return; }
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextMoveTime) { return; }
    if (AI->GetMoveStatus() == EPathFollowingStatus::Moving && RequestedDestination.Equals(Location, 50)) { return; }
    // Actor 목표를 쓰면 벽 뒤 실제 위치를 추적하게 되므로 목격 위치 스냅샷만 사용한다.
    AI->MoveToLocation(Location, AcceptanceRadius, false, true, true, false, nullptr, true);
    RequestedDestination = Location;
    NextMoveTime = Now + 0.35f;
}

void ASPGuardCharacter::ReturnToPatrol()
{
    TargetPlayer = nullptr;
    ListeningRemaining = 0;
    HeardPlayer.Reset();
    GuardState = ESPGuardState::Patrol;
    if (AAIController* AI = Cast<AAIController>(Controller)) { AI->StopMovement(); }
    RequestedDestination = FVector(UE_BIG_NUMBER);
    // 순찰 복귀는 신원 기억을 지우지 않는다.
    ForceNetUpdate();
}

void ASPGuardCharacter::UpdateBehavior(float DeltaSeconds)
{
    AAIController* AI = Cast<AAIController>(Controller);
    if (!AI) { return; }
    GetCharacterMovement()->MaxWalkSpeed = TargetPlayer ? ChaseSpeed : PatrolSpeed;
    if (TargetPlayer)
    {
        if (!IsValid(TargetPlayer) || !TargetPlayer->IsPlayerControlled()) { ReturnToPatrol(); return; }
        const float Distance = FVector::Dist2D(GetActorLocation(), LastSeenLocation);
        if (bTargetVisible)
        {
            if (Distance > 115) { MoveToLocation(LastSeenLocation, 90); }
            else
            {
                AI->StopMovement();
                SetActorRotation(FRotator(0, (LastSeenLocation - GetActorLocation()).Rotation().Yaw, 0));
            }
            return;
        }
        if (GetWorld()->GetTimeSeconds() - LastSightingTime >= LostTargetTimeout)
        {
            ReturnToPatrol(); return;
        }
        if (Distance < 100 || GuardState == ESPGuardState::Searching)
        {
            if (GuardState != ESPGuardState::Searching)
            {
                AI->StopMovement();
                SearchRemaining = SearchTime;
                GuardState = ESPGuardState::Searching;
            }
            SearchRemaining -= DeltaSeconds;
            AddActorWorldRotation(FRotator(0, 50 * DeltaSeconds, 0));
            if (SearchRemaining <= 0) { ReturnToPatrol(); }
        }
        else { MoveToLocation(LastSeenLocation, 60); }
        return;
    }
    if (GuardState == ESPGuardState::Listening)
    {
        AI->StopMovement();
        GetCharacterMovement()->StopMovementImmediately();
        const FVector ToSound = HeardLocation - GetActorLocation();
        if (!ToSound.IsNearlyZero())
        {
            SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(),
                FRotator(0, ToSound.Rotation().Yaw, 0), DeltaSeconds, HearingTurnSpeed));
        }
        return;
    }
    if (GuardState == ESPGuardState::Suspicious)
    {
        AI->StopMovement();
        return;
    }
    if (!PatrolRoute || PatrolRoute->Points.IsEmpty()) { AI->StopMovement(); return; }
    PatrolIndex = FMath::Clamp(PatrolIndex, 0, PatrolRoute->Points.Num() - 1);
    const FVector Goal = PatrolRoute->GetPatrolLocation(PatrolIndex);
    if (FVector::Dist2D(GetActorLocation(), Goal) <= 65)
    {
        AI->StopMovement();
        PatrolWaitRemaining += DeltaSeconds;
        if (PatrolWaitRemaining >= PatrolWaitTime)
        {
            PatrolWaitRemaining = 0;
            if (PatrolRoute->bLoop) { PatrolIndex = (PatrolIndex + 1) % PatrolRoute->Points.Num(); }
            else if (PatrolRoute->Points.Num() > 1)
            {
                if (PatrolIndex + PatrolDirection >= PatrolRoute->Points.Num() || PatrolIndex + PatrolDirection < 0)
                { PatrolDirection *= -1; }
                PatrolIndex += PatrolDirection;
            }
        }
    }
    else { MoveToLocation(Goal, 40); }
}

void ASPGuardCharacter::UpdateFeedback()
{
    if (GetNetMode() == NM_DedicatedServer) { return; }
    const bool bAlert = GuardState == ESPGuardState::Pursuing;
    AlertIndicator->SetVisibility(GuardState != ESPGuardState::Patrol);
    AlertIndicator->SetText(FText::FromString(bAlert ? TEXT("!") : TEXT("?")));
    AlertIndicator->SetTextRenderColor(bAlert ? FColor::Red : FColor::Yellow);
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
        UpdateSight(DeltaSeconds);
        UpdateHearing(DeltaSeconds);
        UpdateBehavior(DeltaSeconds);
    }
    UpdateFeedback();
    DrawVision();
}
