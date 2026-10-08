#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SPGuardCharacter.h"
#include "SPGuardPatrolRoute.h"
#include "SPRestrictedArea.h"
#include "SPGuardAlertSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"

namespace SPGuardTests
{
    struct FWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        FWorld()
        {
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            World->GetWorldSettings()->NotifyBeginPlay();
            if (!World->GetGameState()) { World->SetGameState(World->SpawnActor<AGameStateBase>()); }
            World->GetSubsystem<USPGuardAlertSubsystem>()->StartStage();
        }
        ~FWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        ASPPlayerCharacter* Player(const FVector& Location)
        {
            APlayerController* PC = World->SpawnActor<APlayerController>();
            PC->PlayerState = World->SpawnActor<APlayerState>();
            ASPPlayerCharacter* Pawn = World->SpawnActor<ASPPlayerCharacter>();
            Pawn->SetActorLocation(Location);
            PC->Possess(Pawn);
            Pawn->GetCharacterMovement()->DisableMovement();
            return Pawn;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardSightTest, "SpacePirate.Stealth.SightAndCover",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardSightTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld Fixture;
    ASPGuardCharacter* Guard = Fixture.World->SpawnActor<ASPGuardCharacter>();
    Guard->SetActorLocation(FVector(0, 0, 96));
    ASPPlayerCharacter* Player = Fixture.Player(FVector(400, 0, 96));
    TestTrue(TEXT("Front player visible"), Guard->CanSeePlayer(Player));
    Player->SetActorLocation(FVector(-400, 0, 96));
    TestFalse(TEXT("Behind guard never detected"), Guard->CanSeePlayer(Player));
    Player->SetActorLocation(FVector(400, 600, 96));
    TestFalse(TEXT("Outside horizontal cone"), Guard->CanSeePlayer(Player));
    Player->SetActorLocation(FVector(1500, 0, 96));
    TestFalse(TEXT("Outside sight distance"), Guard->CanSeePlayer(Player));
    Player->SetActorLocation(FVector(400, 0, 96));
    AActor* Wall = Fixture.World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20, 200, 150));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();
    Wall->SetActorLocation(FVector(200, 0, 150));
    TestFalse(TEXT("Opaque cover blocks torso and head"), Guard->CanSeePlayer(Player));
    Wall->Destroy();
    TestTrue(TEXT("Removing cover restores sight"), Guard->CanSeePlayer(Player));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardIdentityTest, "SpacePirate.Stealth.PerPlayerAlertAndMemory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardIdentityTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld Fixture;
    auto* Route = Fixture.World->SpawnActor<ASPGuardPatrolRoute>();
    auto* Area = Route->GetWorld()->SpawnActor<ASPRestrictedArea>();
    Area->Volume->SetBoxExtent(FVector(2000));
    auto* First = Fixture.World->SpawnActor<ASPGuardCharacter>();
    auto* Second = Fixture.World->SpawnActor<ASPGuardCharacter>();
    First->SetActorLocation(FVector(0, 0, 96));
    Second->SetActorLocation(FVector(-1000, 500, 96));
    First->PatrolRoute = Route;
    Second->PatrolRoute = Route;
    auto* A = Fixture.Player(FVector(400, 0, 96));
    auto* B = Fixture.Player(FVector(-400, 0, 96));
    auto* Alerts = Fixture.World->GetSubsystem<USPGuardAlertSubsystem>();
    First->Tick(0.6f);
    TestFalse(TEXT("Partial glimpse does not identify A"), Alerts->IsIdentified(First->IdentityScope, A->GetPlayerState()));
    A->SetActorLocation(FVector(-400, -400, 96));
    First->Tick(0.1f);
    A->SetActorLocation(FVector(400, 0, 96));
    First->Tick(0.6f);
    TestFalse(TEXT("Sight interruption resets confirmation"), Alerts->IsIdentified(First->IdentityScope, A->GetPlayerState()));
    First->Tick(0.5f);
    TestTrue(TEXT("Sustained sight identifies A"), Alerts->IsIdentified(First->IdentityScope, A->GetPlayerState()));
    TestFalse(TEXT("Hidden B remains unidentified"), Alerts->IsIdentified(First->IdentityScope, B->GetPlayerState()));
    TestTrue(TEXT("Second guard receives exactly A"), Second->TargetPlayer == A);
    const FVector Snapshot = Second->LastSeenLocation;
    A->SetActorLocation(FVector(-500, -500, 96));
    First->Tick(0.1f);
    TestTrue(TEXT("Radio location does not follow hidden A"), Second->LastSeenLocation.Equals(Snapshot));
    First->TargetPlayer = nullptr;
    TestTrue(TEXT("Ending pursuit does not erase identity"), Alerts->IsIdentified(First->IdentityScope, A->GetPlayerState()));
    TestFalse(TEXT("Other identity scope does not inherit identity"), Alerts->IsIdentified(TEXT("OtherCar"), A->GetPlayerState()));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardFootstepsTest, "SpacePirate.Stealth.FootstepsAndCrouch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardFootstepsTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld Fixture;
    auto* Route = Fixture.World->SpawnActor<ASPGuardPatrolRoute>();
    auto* Area = Route->GetWorld()->SpawnActor<ASPRestrictedArea>();
    Area->Volume->SetBoxExtent(FVector(2000));
    auto* First = Fixture.World->SpawnActor<ASPGuardCharacter>();
    auto* Second = Fixture.World->SpawnActor<ASPGuardCharacter>();
    First->SetActorLocation(FVector(0, 0, 96));
    Second->SetActorLocation(FVector(1000, 500, 96));
    First->bHearFootsteps = true;
    First->PatrolRoute = Route;
    Second->PatrolRoute = Route;
    auto* A = Fixture.Player(FVector(-400, 0, 96));
    auto* B = Fixture.Player(FVector(0, -600, 96));
    auto* Movement = A->GetCharacterMovement();
    Movement->SetMovementMode(MOVE_Walking);
    TestFalse(TEXT("Standing still is silent"), First->CanHearPlayer(A));
    Movement->Velocity = FVector(200, 0, 0);
    TestTrue(TEXT("Normal walking behind guard is audible"), First->CanHearPlayer(A));
    A->bIsCrouched = true;
    TestFalse(TEXT("Crouched movement stays silent"), First->CanHearPlayer(A));
    A->bIsCrouched = false;
    Movement->SetMovementMode(MOVE_Falling);
    TestFalse(TEXT("Airborne movement has no footsteps"), First->CanHearPlayer(A));
    Movement->SetMovementMode(MOVE_Walking);
    A->SetActorLocation(FVector(-900, 0, 96));
    TestFalse(TEXT("Out of hearing range"), First->CanHearPlayer(A));
    A->SetActorLocation(FVector(-400, 0, 96));
    auto* Alerts = Fixture.World->GetSubsystem<USPGuardAlertSubsystem>();
    First->Tick(0.25f);
    TestTrue(TEXT("Sound starts listening and turning"), First->GuardState == ESPGuardState::Listening
        && !First->GetActorForwardVector().Equals(FVector::ForwardVector));
    TestNull(TEXT("Sound alone does not identify a target"), First->TargetPlayer.Get());
    TestNull(TEXT("No radio alert before visual confirmation"), Second->TargetPlayer.Get());
    for (int32 I = 0; I < 10; ++I) { First->Tick(0.25f); }
    TestTrue(TEXT("Turning enables normal visual identification"), Alerts->IsIdentified(First->IdentityScope, A->GetPlayerState()));
    TestTrue(TEXT("Radio still identifies only walking A"), Second->TargetPlayer == A);
    TestFalse(TEXT("Silent B remains unidentified"), Alerts->IsIdentified(First->IdentityScope, B->GetPlayerState()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardOccludedFootstepsTest, "SpacePirate.Stealth.OccludedFootsteps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardOccludedFootstepsTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld Fixture;
    auto* Route = Fixture.World->SpawnActor<ASPGuardPatrolRoute>();
    auto* Area = Route->GetWorld()->SpawnActor<ASPRestrictedArea>();
    Area->Volume->SetBoxExtent(FVector(2000));
    auto* Guard = Fixture.World->SpawnActor<ASPGuardCharacter>();
    Guard->SetActorLocation(FVector(0, 0, 96));
    Guard->bHearFootsteps = true;
    Guard->PatrolRoute = Route;
    auto* Player = Fixture.Player(FVector(-400, 0, 96));
    Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Player->GetCharacterMovement()->Velocity = FVector(200, 0, 0);
    AActor* Wall = Fixture.World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20, 200, 150));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->RegisterComponent();
    Wall->SetActorLocation(FVector(-200, 0, 150));
    for (int32 I = 0; I < 16; ++I) { Guard->Tick(0.25f); }
    TestTrue(TEXT("Wall permits a direction-only sound response"), Guard->GuardState == ESPGuardState::Listening);
    TestFalse(TEXT("Wall still blocks sight after turning"), Guard->CanSeePlayer(Player));
    TestNull(TEXT("Hearing through cover never identifies the player"), Guard->TargetPlayer.Get());
    const FVector LastSound = Guard->HeardLocation;
    Player->bIsCrouched = true;
    Player->SetActorLocation(FVector(-400, 300, 96));
    Guard->Tick(0.25f);
    TestTrue(TEXT("Crouching stops sound-position updates"), Guard->HeardLocation.Equals(LastSound));
    for (int32 I = 0; I < 12; ++I) { Guard->Tick(0.25f); }
    TestTrue(TEXT("Unconfirmed sound expires back to patrol"), Guard->GuardState == ESPGuardState::Patrol);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardStageResetTest, "SpacePirate.Stealth.StageResetAndMissingIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardStageResetTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld Fixture;
    auto* Route = Fixture.World->SpawnActor<ASPGuardPatrolRoute>();
    auto* Area = Route->GetWorld()->SpawnActor<ASPRestrictedArea>();
    Area->Volume->SetBoxExtent(FVector(2000));
    auto* Guard = Fixture.World->SpawnActor<ASPGuardCharacter>();
    Guard->SetActorLocation(FVector(0, 0, 96));
    Guard->PatrolRoute = Route;
    auto* Player = Fixture.Player(FVector(400, 0, 96));
    auto* Alerts = Fixture.World->GetSubsystem<USPGuardAlertSubsystem>();
    Guard->Tick(0.6f);
    Alerts->RestartStage();
    Guard->Tick(0.6f);
    TestFalse(TEXT("Restart clears partial confirmation time"), Alerts->IsIdentified(Guard->IdentityScope, Player->GetPlayerState()));
    Guard->Tick(0.5f);
    TestTrue(TEXT("Fresh continuous confirmation still identifies"), Alerts->IsIdentified(Guard->IdentityScope, Player->GetPlayerState()));
    Alerts->RestartStage();
    Player->SetPlayerState(nullptr);
    Guard->Tick(1.2f);
    TestNull(TEXT("Rejected missing-identity report cannot create local pursuit"), Guard->TargetPlayer.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardAnonymousLifecycleTest, "SpacePirate.Stealth.Investigation.AnonymousLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardAnonymousLifecycleTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld F;
    auto* S = F.World->GetSubsystem<USPGuardAlertSubsystem>();
    auto* G = F.World->SpawnActor<ASPGuardCharacter>();
    G->SetActorLocation(FVector(0, 0, 96));
    G->SearchTime = 1;
    G->bHearFootsteps = false;
    auto* Normal = F.Player(FVector(250, 0, 96));
    const auto C = S->MakeIncidentContext(FVector(0, 0, 0), G->AlertGroup);
    S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise, C);
    TestTrue(TEXT("Targetless dispatch starts travel state"), !G->TargetPlayer && G->GuardState == ESPGuardState::Investigating);
    G->Tick(0.4f);
    TestTrue(TEXT("Arrival starts separate scene search"), G->GuardState == ESPGuardState::SceneSearching);
    auto ChangedRetry = C; ChangedRetry.Location = FVector(1500, 0, 0);
    TestTrue(TEXT("Guard retry merges without replacing snapshot"), G->ReceiveAnonymousIncident(ChangedRetry) && G->InvestigationLocation.Equals(C.Location));
    G->Tick(0.4f);
    TestTrue(TEXT("Normal visible player remains hidden during scene search"), G->CanSeePlayer(Normal)
        && !S->IsIdentified(G->IdentityScope, Normal->GetPlayerState()) && !G->TargetPlayer);
    G->Tick(0.3f);
    TestTrue(TEXT("Retry never prolongs search; completion returns to patrol"), G->GuardState == ESPGuardState::Patrol
        && !G->bInvestigatingAnonymousIncident && G->LastInvestigation.Result == ESPGuardInvestigationResult::Completed
        && G->LastInvestigation.IncidentId == C.IncidentId);
    G->ReceiveAnonymousIncident(C);
    TestFalse(TEXT("Completed same ID does not start another search"), G->bInvestigatingAnonymousIncident);
    auto New = S->MakeIncidentContext(FVector(50, 0, 0), G->AlertGroup);
    S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise, New);
    TestTrue(TEXT("New event gets independent lifecycle"), G->bInvestigatingAnonymousIncident && G->InvestigationIncidentId == New.IncidentId);
    const auto Next = S->MakeIncidentContext(FVector(-50, 0, 0), G->AlertGroup);
    S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise, Next);
    TestTrue(TEXT("New anonymous event records superseded previous event"), G->LastInvestigation.IncidentId == New.IncidentId
        && G->LastInvestigation.Result == ESPGuardInvestigationResult::Superseded && G->InvestigationIncidentId == Next.IncidentId);
    S->RestartStage();
    TestTrue(TEXT("Stage reset clears active and completed response"), !G->InvestigationIncidentId.IsValid()
        && G->LastInvestigation.Result == ESPGuardInvestigationResult::None && !G->TargetPlayer);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardInvestigationCrimeTest, "SpacePirate.Stealth.Investigation.CrimePriority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardInvestigationCrimeTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld F;
    auto* S = F.World->GetSubsystem<USPGuardAlertSubsystem>();
    auto* G = F.World->SpawnActor<ASPGuardCharacter>();
    G->SetActorLocation(FVector(0, 0, 96));
    auto* Player = F.Player(FVector(400, 0, 96));
    auto* Route = F.World->SpawnActor<ASPGuardPatrolRoute>();
    auto* Area = Route->GetWorld()->SpawnActor<ASPRestrictedArea>();
    Area->Volume->SetBoxExtent(FVector(1000));
    G->PatrolRoute = Route;
    const auto C = S->MakeIncidentContext(FVector(-400, 0, 0), G->AlertGroup);
    S->SubmitAnonymousIncident(ESPStealthIncident::LaserContact, C);
    G->Tick(0.4f);
    TestTrue(TEXT("Visual confirmation pauses investigation"), G->GuardState == ESPGuardState::Suspicious && !G->TargetPlayer);
    G->Tick(0.7f);
    TestTrue(TEXT("Real trespass confirms actor and interrupts anonymous job"), G->TargetPlayer == Player
        && G->GuardState == ESPGuardState::Pursuing && S->IsIdentified(G->IdentityScope, Player->GetPlayerState())
        && G->LastInvestigation.Result == ESPGuardInvestigationResult::InterruptedBySighting);
    const FVector Seen = G->LastSeenLocation;
    auto R = S->SubmitAnonymousIncident(ESPStealthIncident::LaserContact, S->MakeIncidentContext(FVector(2000,0,0),G->AlertGroup));
    TestTrue(TEXT("Anonymous cannot replace confirmed target"), R.DispatchedGuards == 0 && G->TargetPlayer == Player && G->LastSeenLocation.Equals(Seen));
    Player->SetActorLocation(FVector(-2000, -2000, 96));
    G->Tick(0.1f);
    TestTrue(TEXT("Hidden target uses frozen last sighting"), G->GuardState == ESPGuardState::MovingToLastSeen && G->LastSeenLocation.Equals(Seen));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardNoNavTest, "SpacePirate.Stealth.Investigation.MissingNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardNoNavTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld F;
    auto* S = F.World->GetSubsystem<USPGuardAlertSubsystem>();
    auto* G = F.World->SpawnActor<ASPGuardCharacter>();
    G->SpawnDefaultController();
    G->MaxMoveRetries = 2;
    G->MoveRetryInterval = 1;
    const auto C = S->MakeIncidentContext(FVector(1500,0,0),G->AlertGroup);
    S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise,C);
    G->Tick(0.1f);
    TestEqual(TEXT("One nav request on first tick"),G->MoveRequestCount,1);
    for (int32 I=0; I<5; ++I) { G->Tick(0.1f); }
    TestEqual(TEXT("Retry delay prevents per-tick requests"),G->MoveRequestCount,1);
    G->Tick(1.0f); G->Tick(1.0f);
    TestTrue(TEXT("Missing NavMesh terminates after bounded retries"),G->GuardState == ESPGuardState::Patrol && !G->TargetPlayer
        && !G->bInvestigatingAnonymousIncident && G->LastInvestigation.Result == ESPGuardInvestigationResult::Failed
        && G->LastInvestigation.Failure == ESPGuardMoveFailure::NoNavigation && G->LastInvestigation.MoveRequests == 3);
    for (int32 I=0; I<20; ++I) { G->Tick(0.1f); }
    TestTrue(TEXT("Failed incident is not relaunched every tick"),G->LastInvestigation.IncidentId == C.IncidentId
        && G->LastInvestigation.MoveRequests == 3 && !G->bInvestigatingAnonymousIncident);
    return true;
}

#endif
