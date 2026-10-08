#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SPLaserSecurityDevice.h"
#include "SPGuardAlertSubsystem.h"
#include "SPGuardCharacter.h"
#include "SPStealthGameStateComponent.h"
#include "SPStealthActivityComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"

namespace SPLaserTests
{
    struct FWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        USPGuardAlertSubsystem* S;
        ASPPlayerCharacter* A;
        ASPPlayerCharacter* B;
        ASPLaserSecurityDevice* Laser;
        FWorld()
        {
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
            World->GetWorldSettings()->MaxUndilatedFrameTime = 10.f; // Deliberate long-frame timing tests.
            if (!World->GetGameState()) World->SetGameState(World->SpawnActor<AGameStateBase>());
            S = World->GetSubsystem<USPGuardAlertSubsystem>(); S->StartStage();
            A = Player(FVector(0,-200,96)); B = Player(FVector(1000,-200,96));
            Laser = Sensor(); Step();
        }
        ~FWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
        ASPPlayerCharacter* Player(FVector Location)
        {
            auto* PC = World->SpawnActor<APlayerController>(); PC->PlayerState = World->SpawnActor<APlayerState>();
            auto* P = World->SpawnActor<ASPPlayerCharacter>(); P->SetActorLocation(Location); PC->Possess(P);
            P->GetCharacterMovement()->DisableMovement(); S->RegisterPlayer(PC->PlayerState); return P;
        }
        ASPLaserSecurityDevice* Sensor()
        {
            auto* D = World->SpawnActor<ASPLaserSecurityDevice>();
            D->SetActorTickEnabled(false); // Native transient test worlds do not dispatch actor ticks reliably.
            D->LocalStart = FVector(-250,0,100); D->LocalEnd = FVector(250,0,100);
            D->DispatchGroup = TEXT("LaserTest"); D->RefreshDevice(); return D;
        }
        void Step(float Delta=.016f)
        {
            World->Tick(LEVELTICK_All, Delta); // Advance the actual server clock and physics scene.
            for (TActorIterator<ASPLaserSecurityDevice> It(World); It; ++It) It->Tick(Delta);
        }
        bool Hidden() { return !S->IsIdentified(TEXT("StageSecurity"), A->GetPlayerState()) &&
            !S->IsIdentified(TEXT("StageSecurity"), B->GetPlayerState()) && !S->GetSecurityState()->GetAlarmState().bGlobalAlarm; }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLaserScheduleTest, "SpacePirate.Stealth.Laser.ScheduleAndAuthoring",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLaserScheduleTest::RunTest(const FString& Parameters)
{
    SPLaserTests::FWorld F;
    auto* L = F.Laser; L->Mode = ESPLaserMode::Periodic; L->OnSeconds = 4; L->OffSeconds = 4; L->WarningSeconds = 1;
    TestTrue(TEXT("Start is on"), L->GetPhaseAtElapsed(0) == ESPLaserPhase::On);
    TestTrue(TEXT("Off warning is still live"), L->GetPhaseAtElapsed(3.1) == ESPLaserPhase::WarningOff);
    TestTrue(TEXT("Off boundary is safe"), L->GetPhaseAtElapsed(4) == ESPLaserPhase::Off);
    TestTrue(TEXT("On warning remains safe"), L->GetPhaseAtElapsed(7.1) == ESPLaserPhase::WarningOn);
    TestTrue(TEXT("Cycle restarts at exact boundary"), L->GetPhaseAtElapsed(8) == ESPLaserPhase::On);
    L->InitialPhaseSeconds = -1;
    TestTrue(TEXT("Negative phase offsets wrap"), L->GetPhaseAtElapsed(0) == ESPLaserPhase::WarningOn);
    L->bUseBeamLength = true; L->BeamLength = 300; L->RefreshDevice();
    TestTrue(TEXT("Explicit length fits local endpoint direction"), FMath::IsNearlyEqual(FVector::Distance(L->GetBeamStart(),L->GetBeamEnd()),300.));
    auto* Receiver = F.World->SpawnActor<AActor>();
    auto* Root = NewObject<USceneComponent>(Receiver); Receiver->SetRootComponent(Root); Root->RegisterComponent();
    Receiver->SetActorLocation(FVector(0,400,200)); L->ReceiverActor = Receiver; L->bUseBeamLength = false; L->RefreshDevice();
    TestTrue(TEXT("Receiver actor is usable as endpoint"), L->GetBeamEnd().Equals(Receiver->GetActorLocation()));
    L->bEnabled = false; L->ResetDevice();
    TestFalse(TEXT("Disabled sensor is safe"), L->IsBeamActive());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLaserAnonymousTest, "SpacePirate.Stealth.Laser.AnonymousDispatchAndIndependentWitness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLaserAnonymousTest::RunTest(const FString& Parameters)
{
    SPLaserTests::FWorld F;
    auto* Guard = F.World->SpawnActor<ASPGuardCharacter>(); Guard->AlertGroup = TEXT("LaserTest");
    Guard->SetActorLocation(FVector(-400,0,96)); Guard->SightDistance = 1;
    F.A->SetActorLocation(FVector(0,0,96)); F.Step();
    TestEqual(TEXT("Contact produces one accepted incident"), F.Laser->IncidentCount, 1);
    TestTrue(TEXT("Receipt is laser evidence without player identity"), F.Laser->LastIncident.Kind == ESPStealthIncident::LaserContact && !F.Laser->LastIncident.Player.IsValid());
    TestTrue(TEXT("Anonymous laser changes neither player's identity nor alarm"), F.Hidden());
    TestTrue(TEXT("Guard receives only floor sensor location"), Guard->bInvestigatingAnonymousIncident && !Guard->TargetPlayer &&
        Guard->InvestigationLocation.Equals(F.Laser->GetActorLocation()));
    Guard->SightDistance = 1200; Guard->SetActorRotation(FRotator::ZeroRotator);
    F.A->GetStealthActivity()->BeginCrime(ESPCrimeKind::VaultWork, F.Laser);
    Guard->Tick(1.1f);
    TestTrue(TEXT("Later independent visual crime identifies only actor A"), F.S->IsIdentified(TEXT("StageSecurity"),F.A->GetPlayerState()) &&
        !F.S->IsIdentified(TEXT("StageSecurity"),F.B->GetPlayerState()) && Guard->TargetPlayer == F.A);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLaserCooldownTest, "SpacePirate.Stealth.Laser.CooldownReentryAndOtherSensor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLaserCooldownTest::RunTest(const FString& Parameters)
{
    SPLaserTests::FWorld F; auto* L = F.Laser;
    F.A->SetActorLocation(FVector(0,0,96)); F.Step(); const FGuid First = L->LastIncident.Context.IncidentId;
    F.Step(.5f);
    TestEqual(TEXT("Continuous contact does not spam each frame"), L->SubmissionCount, 1);
    F.Step(1.6f);
    TestTrue(TEXT("Continuous retry keeps incident ID and merges"), L->SubmissionCount == 2 && L->IncidentCount == 1 &&
        L->LastIncident.Context.IncidentId == First && L->LastIncident.Result == ESPStealthResult::Duplicate && L->ContinuousCount == 1);
    F.A->SetActorLocation(FVector(0,-200,96)); F.Step();
    F.A->SetActorLocation(FVector(0,0,96)); F.Step();
    TestTrue(TEXT("Reentry creates a distinct queued occurrence under same device gate"), L->ReentryCount == 1 && L->PendingCount == 1 && L->SubmissionCount == 2);
    F.A->SetActorLocation(FVector(0,-200,96)); F.Step(); F.Step(2.1f);
    TestTrue(TEXT("Reentry snapshot survives leaving before cooldown expires"), L->IncidentCount == 2 && L->LastIncident.Context.IncidentId != First && L->PendingCount == 0);
    auto* Other = F.Sensor(); Other->DeviceId = TEXT("Other"); F.Step();
    F.A->SetActorLocation(FVector(0,0,96)); F.Step();
    TestEqual(TEXT("Another sensor has an independent gate"), Other->IncidentCount, 1);
    TestTrue(TEXT("All these events stay anonymous"), F.Hidden());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLaserSweepTest, "SpacePirate.Stealth.Laser.ThinFastCrossAndOffInterval",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLaserSweepTest::RunTest(const FString& Parameters)
{
    SPLaserTests::FWorld F; auto* L = F.Laser;
    L->DetectionThickness = .2f; L->RefreshDevice(); F.Step();
    F.A->SetActorLocation(FVector(0,200,96)); F.Step();
    TestTrue(TEXT("Entire thin beam crossing in one frame is detected"), L->IncidentCount == 1 && L->FastCrossCount == 1);
    L->Mode = ESPLaserMode::Periodic; L->OnSeconds = 4; L->OffSeconds = 4; L->InitialPhaseSeconds = 4.1f; L->ResetDevice(); F.Step();
    F.A->SetActorLocation(FVector(0,-200,96)); F.Step();
    TestEqual(TEXT("Swept crossing while off is safe"), L->IncidentCount, 0);
    F.Step(4.f);
    TestEqual(TEXT("Activation does not retroactively detect a completed off crossing"), L->IncidentCount, 0);
    L->InitialPhaseSeconds = 7.9f; L->ResetDevice(); F.Step();
    F.A->SetActorLocation(FVector(0,0,96)); F.Step(.02f);
    TestEqual(TEXT("Entering during warning-on is safe"), L->IncidentCount, 0);
    F.Step(.2f);
    TestTrue(TEXT("Power-on detects player already inside"), L->IncidentCount == 1 && L->ActivationCount == 1);
    // A hitch can contain an entire on window although both endpoint snapshots are off.
    F.A->SetActorLocation(FVector(0,-200,96)); L->OnSeconds=.2f; L->OffSeconds=.2f; L->InitialPhaseSeconds=.3f; L->ResetDevice(); F.Step(.001f);
    F.A->SetActorLocation(FVector(0,200,96)); F.Step(.4f);
    TestTrue(TEXT("Time-clipped sweep catches active window inside a long frame"), L->IncidentCount == 1);
    F.A->SetActorLocation(FVector(0,0,96)); L->InitialPhaseSeconds=0; L->ResetDevice(); F.Step(.001f); F.Step(.4f);
    TestTrue(TEXT("A complete power cycle inside one hitch still creates a distinct occupied activation"),
        L->ActivationCount == 2 && L->ContactCount == 2 && L->PendingCount == 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLaserShapeTest, "SpacePirate.Stealth.Laser.CapsuleShapeAndVisualIndependence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLaserShapeTest::RunTest(const FString& Parameters)
{
    SPLaserTests::FWorld F; auto* L = F.Laser;
    L->LocalStart.Z = L->LocalEnd.Z = 145; L->RefreshDevice();
    F.A->GetCapsuleComponent()->SetCapsuleHalfHeight(56); F.A->SetActorLocation(FVector(0,-200,56)); F.Step();
    F.A->SetActorLocation(FVector(0,200,56)); F.Step();
    TestEqual(TEXT("Actual crouched 112cm capsule clears 145cm beam"), L->IncidentCount, 0);
    F.A->GetCapsuleComponent()->SetCapsuleHalfHeight(96); F.A->SetActorLocation(FVector(0,200,96)); F.Step();
    F.A->SetActorLocation(FVector(0,-200,96)); F.Step();
    TestEqual(TEXT("Standing 192cm capsule hits same beam"), L->IncidentCount, 1);
    L->ResetDevice(); F.A->SetActorLocation(FVector(0,-200,250)); F.Step();
    F.A->SetActorLocation(FVector(0,200,250)); F.Step();
    TestEqual(TEXT("Airborne capsule entirely above beam clears"), L->IncidentCount, 0);
    L->BeamVisual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
    L->BeamVisual->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); L->VisualThickness = 500; L->RefreshDevice();
    TestTrue(TEXT("Visual replacement cannot acquire gameplay collision"), L->BeamVisual->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        FMath::IsNearlyEqual(L->DetectionVolume->GetUnscaledBoxExtent().Y,2.));
    F.A->SetActorLocation(FVector(0,-200,96)); F.Step(); F.A->SetActorLocation(FVector(0,200,96)); F.Step();
    TestEqual(TEXT("Same true capsule contact after replacement"), L->IncidentCount, 1);
    L->ResetDevice(); F.A->SetActorRotation(FRotator(0,0,90)); F.A->SetActorLocation(FVector(0,-200,145)); F.Step();
    F.A->SetActorLocation(FVector(0,200,145)); F.Step();
    TestEqual(TEXT("Gravity-rotated capsule uses component quaternion"), L->IncidentCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLaserLifecycleTest, "SpacePirate.Stealth.Laser.StageLifecycleAndSensorMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLaserLifecycleTest::RunTest(const FString& Parameters)
{
    SPLaserTests::FWorld F; auto* L = F.Laser;
    F.A->SetActorLocation(FVector(0,0,96)); F.Step();
    F.S->EndStage(); F.Step();
    TestTrue(TEXT("Ended stage deactivates beam and clears queued work"), !L->IsBeamActive() && L->PendingCount == 0);
    const FGuid Old = L->StageId; F.S->StartStage(); F.Step();
    TestTrue(TEXT("New stage clears old episodes, samples current occupancy once"), L->StageId != Old && L->IncidentCount == 1 && L->ActivationCount == 1);
    F.S->RestartStage(); F.Step();
    TestTrue(TEXT("Restart also re-arms occupied sensor anonymously"), L->IncidentCount == 1 && F.Hidden());
    F.A->SetActorLocation(FVector(0,200,96)); L->ResetDevice(); F.Step();
    L->SetActorLocation(FVector(0,400,0)); F.Step();
    TestEqual(TEXT("Moving the sensor across stationary player is not a player sweep"), L->IncidentCount, 0);
    L->SetActorLocation(FVector(0,200,0)); F.Step();
    TestEqual(TEXT("Moving sensor onto current player still evaluates occupancy"), L->IncidentCount, 1);
    F.A->GetController()->UnPossess(); F.Step(); F.Step(2.1f);
    TestEqual(TEXT("Unpossessed pawn cannot keep generating continuous calls"), L->SubmissionCount, 1);
    return true;
}
#endif
