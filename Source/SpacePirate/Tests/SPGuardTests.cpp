#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SPGuardCharacter.h"
#include "SPGuardPatrolRoute.h"
#include "SPGuardAlertSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
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
    Route->RestrictedArea->SetBoxExtent(FVector(2000));
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
    TestFalse(TEXT("Partial glimpse does not identify A"), Alerts->IsIdentified(First->AlertGroup, A->GetPlayerState()));
    A->SetActorLocation(FVector(-400, -400, 96));
    First->Tick(0.1f);
    A->SetActorLocation(FVector(400, 0, 96));
    First->Tick(0.6f);
    TestFalse(TEXT("Sight interruption resets confirmation"), Alerts->IsIdentified(First->AlertGroup, A->GetPlayerState()));
    First->Tick(0.5f);
    TestTrue(TEXT("Sustained sight identifies A"), Alerts->IsIdentified(First->AlertGroup, A->GetPlayerState()));
    TestFalse(TEXT("Hidden B remains unidentified"), Alerts->IsIdentified(First->AlertGroup, B->GetPlayerState()));
    TestTrue(TEXT("Second guard receives exactly A"), Second->TargetPlayer == A);
    const FVector Snapshot = Second->LastSeenLocation;
    A->SetActorLocation(FVector(-500, -500, 96));
    First->Tick(0.1f);
    TestTrue(TEXT("Radio location does not follow hidden A"), Second->LastSeenLocation.Equals(Snapshot));
    First->TargetPlayer = nullptr;
    TestTrue(TEXT("Ending pursuit does not erase identity"), Alerts->IsIdentified(First->AlertGroup, A->GetPlayerState()));
    TestFalse(TEXT("Other radio group does not inherit identity"), Alerts->IsIdentified(TEXT("OtherCar"), A->GetPlayerState()));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardFootstepsTest, "SpacePirate.Stealth.FootstepsAndCrouch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardFootstepsTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld Fixture;
    auto* Route = Fixture.World->SpawnActor<ASPGuardPatrolRoute>();
    Route->RestrictedArea->SetBoxExtent(FVector(2000));
    auto* First = Fixture.World->SpawnActor<ASPGuardCharacter>();
    auto* Second = Fixture.World->SpawnActor<ASPGuardCharacter>();
    First->SetActorLocation(FVector(0, 0, 96));
    Second->SetActorLocation(FVector(1000, 500, 96));
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
    TestTrue(TEXT("Turning enables normal visual identification"), Alerts->IsIdentified(First->AlertGroup, A->GetPlayerState()));
    TestTrue(TEXT("Radio still identifies only walking A"), Second->TargetPlayer == A);
    TestFalse(TEXT("Silent B remains unidentified"), Alerts->IsIdentified(First->AlertGroup, B->GetPlayerState()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGuardOccludedFootstepsTest, "SpacePirate.Stealth.OccludedFootsteps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGuardOccludedFootstepsTest::RunTest(const FString& Parameters)
{
    SPGuardTests::FWorld Fixture;
    auto* Route = Fixture.World->SpawnActor<ASPGuardPatrolRoute>();
    Route->RestrictedArea->SetBoxExtent(FVector(2000));
    auto* Guard = Fixture.World->SpawnActor<ASPGuardCharacter>();
    Guard->SetActorLocation(FVector(0, 0, 96));
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
#endif
