#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SPRestrictedArea.h"
#include "SPGuardCharacter.h"
#include "SPGuardPatrolRoute.h"
#include "SPGuardAlertSubsystem.h"
#include "SPStealthActivityComponent.h"
#include "SPStealthObserverComponent.h"
#include "SPStealthGameStateComponent.h"
#include "SPInteractableComponent.h"
#include "SPInteractorComponent.h"
#include "SPInventoryComponent.h"
#include "SPPlayerStatusComponent.h"
#include "SPLootBundle.h"
#include "SPCargo.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/WorldSettings.h"

namespace SPCrimeTests
{
    struct FWorld
    {
        UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
        USPGuardAlertSubsystem* S;
        ASPPlayerCharacter* A;
        ASPPlayerCharacter* B;
        FWorld()
        {
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
            if (!World->GetGameState()) { World->SetGameState(World->SpawnActor<AGameStateBase>()); }
            S=World->GetSubsystem<USPGuardAlertSubsystem>(); S->StartStage();
            A=Player(FVector(400,0,96)); B=Player(FVector(400,300,96));
        }
        ~FWorld() { World->EndPlay(EEndPlayReason::Quit); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
        ASPPlayerCharacter* Player(FVector Location)
        {
            auto* PC=World->SpawnActor<APlayerController>(); PC->PlayerState=World->SpawnActor<APlayerState>();
            auto* P=World->SpawnActor<ASPPlayerCharacter>(); P->SetActorLocation(Location); PC->Possess(P);
            P->GetCharacterMovement()->DisableMovement(); S->RegisterPlayer(PC->PlayerState); return P;
        }
        USPStealthObserverComponent* Observer(FVector Location=FVector(0,0,160))
        {
            auto* Actor=World->SpawnActor<AActor>();
            auto* C=NewObject<USPStealthObserverComponent>(Actor); Actor->SetRootComponent(C); Actor->AddInstanceComponent(C);
            C->bAutoObserve=false; C->RegisterComponent(); C->SetWorldLocation(Location); return C;
        }
        USPInteractableComponent* Interaction(ESPCrimeKind Kind=ESPCrimeKind::LootPacking,float Duration=2)
        {
            auto* Actor=World->SpawnActor<AActor>(); Actor->SetReplicates(true);
            auto* Root=NewObject<USceneComponent>(Actor); Actor->SetRootComponent(Root); Root->RegisterComponent();
            Actor->SetActorLocation(A->GetActorLocation()+FVector(80,0,0));
            auto* C=NewObject<USPInteractableComponent>(Actor); Actor->AddInstanceComponent(C);
            C->CrimeKind=Kind; C->HoldDuration=Duration; C->RegisterComponent(); return C;
        }
        AActor* Wall()
        {
            auto* Actor=World->SpawnActor<AActor>();
            auto* Box=NewObject<UBoxComponent>(Actor); Actor->SetRootComponent(Box); Box->SetBoxExtent(FVector(20,500,200));
            Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Actor->SetActorLocation(FVector(200,0,150));return Actor;
        }
        bool Exposed(ASPPlayerCharacter* P) { return S->IsIdentified(TEXT("StageSecurity"),P->GetPlayerState()); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPIndependentAreaTest,"SpacePirate.Stealth.Crime.IndependentArea",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSPIndependentAreaTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* Area=F.World->SpawnActor<ASPRestrictedArea>(); Area->Volume->SetBoxExtent(FVector(150,300,200));
    Area->SetActorLocation(FVector(400,0,0)); Area->SetActorRotation(FRotator(0,30,0));
    auto* Route=F.World->SpawnActor<ASPGuardPatrolRoute>();
    const FVector AreaLocation=Area->GetActorLocation();
    TestTrue(TEXT("Shared volume finds restricted player independently of route"),ASPRestrictedArea::FindAtLocation(F.A,F.A->GetActorLocation())==Area);
    Route->SetActorLocation(FVector(-3000,4000,0)); Route->Points.Reset();
    TestTrue(TEXT("Changing patrol cannot move or remove restriction"),Area->GetActorLocation()==AreaLocation && Area->ContainsLocation(F.A->GetActorLocation()));
    Area->SetRestrictedEnabled(false);
    TestNull(TEXT("Disabled area has no restriction"),ASPRestrictedArea::FindAtLocation(F.A,F.A->GetActorLocation()));
    Area->SetRestrictedEnabled(true);
    F.A->GetStealthActivity()->TickComponent(.1f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Server UI area follows shared query"),F.A->GetStealthActivity()->GetCurrentArea()==Area);
    F.A->SetActorLocation(FVector(-2000,0,96)); F.A->GetStealthActivity()->TickComponent(.1f,LEVELTICK_All,nullptr);
    TestNull(TEXT("Leaving reports public area"),F.A->GetStealthActivity()->GetCurrentArea());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPSharedObserverTest,"SpacePirate.Stealth.Crime.ObserverIsolation",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSPSharedObserverTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* One=F.Observer(); auto* Two=F.Observer(FVector(0,-40,160));
    int32 Witnesses=0;
    One->OnWitnessConfirmedNative.AddLambda([&](const FSPStealthWitness&){++Witnesses;});
    auto* Source=F.Interaction();
    const auto A=F.A->GetStealthActivity()->BeginCrime(ESPCrimeKind::LootPacking,Source);
    const auto B=F.B->GetStealthActivity()->BeginCrime(ESPCrimeKind::VaultWork,Source);
    TestTrue(TEXT("Per-source registration is idempotent"),A==F.A->GetStealthActivity()->BeginCrime(ESPCrimeKind::LootPacking,Source));
    TestFalse(TEXT("A partial glimpse"),One->SamplePlayer(F.A,.6f).bConfirmed);
    TestFalse(TEXT("B does not inherit A elapsed time"),One->SamplePlayer(F.B,.6f).bConfirmed);
    TestFalse(TEXT("Second observer does not inherit first observer time"),Two->SamplePlayer(F.A,.6f).bConfirmed);
    TestTrue(TEXT("A continuous one second emits a witness"),One->SamplePlayer(F.A,.4f).bConfirmed);
    TestEqual(TEXT("Exactly one completion"),Witnesses,1);
    TestFalse(TEXT("Observation component does not apply response policy"),F.Exposed(F.A));
    auto* Wall=F.Wall();
    TestFalse(TEXT("Cover clears this observer/player timer"),Two->SamplePlayer(F.A,.4f).bVisible);
    Wall->Destroy();
    TestFalse(TEXT("Restored sight starts fresh"),Two->SamplePlayer(F.A,.6f).bConfirmed);
    F.A->GetStealthActivity()->EndCrime(A);
    F.A->GetStealthActivity()->BeginCrime(ESPCrimeKind::LootPacking,Source);
    TestFalse(TEXT("Cancel and restart between samples cannot reuse elapsed time"),Two->SamplePlayer(F.A,.5f).bConfirmed);
    TestTrue(TEXT("B retains its own independent timer"),One->SamplePlayer(F.B,.4f).bConfirmed);
    Two->SetWorldRotation(FRotator(0,180,0));
    TestFalse(TEXT("Observer-local eye direction is respected"),Two->CanSeePlayer(F.A));
    Two->SetWorldLocation(FVector(400,0,800)); Two->SetWorldRotation(FRotator(-80,0,0));
    TestTrue(TEXT("Downward CCTV eye can use the same visibility rules"),Two->CanSeePlayer(F.A));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInstantCrimeTest,"SpacePirate.Stealth.Crime.InstantMoment",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSPInstantCrimeTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* G=F.World->SpawnActor<ASPGuardCharacter>(); G->SetActorLocation(FVector(0,0,96));
    auto* Source=F.Interaction(ESPCrimeKind::SuppressionTool,0);
    Source->bInstantCrime=true;
    auto* Wall=F.Wall();
    const FGuid Hidden=F.A->GetStealthActivity()->ReportInstantCrime(ESPCrimeKind::SuppressionTool,Source);
    TestFalse(TEXT("Action behind cover cannot identify"),F.Exposed(F.A));
    Wall->Destroy(); G->Observer->ObserveInstantCrime(F.A,ESPCrimeKind::SuppressionTool,Hidden);
    G->Tick(.1f);
    TestFalse(TEXT("Becoming visible after the action is not retroactive evidence"),F.Exposed(F.A));
    Source->TryInteract(F.A,250);
    TestTrue(TEXT("Visible instant action identifies immediately without a one-second Tick"),F.Exposed(F.A)&&G->TargetPlayer==F.A);
    TestFalse(TEXT("B stays hidden"),F.Exposed(F.B));
    const auto History=F.S->GetRecentIncidents();
    TestTrue(TEXT("Crime category and instant incident recorded"),History.Last().Context.CrimeKind==ESPCrimeKind::SuppressionTool
        && History.Last().Kind==ESPStealthIncident::InstantCrimeWitnessed);
    TestTrue(TEXT("Instant action leaves no sustained registration"),F.A->GetStealthActivity()->GetActiveCrimes().IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPCrimeLifecycleTest,"SpacePirate.Stealth.Crime.InteractionCleanup",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSPCrimeLifecycleTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* Work=F.Interaction(); auto* Activity=F.A->GetStealthActivity();
    int32 Started=0,Cancelled=0,Completed=0;
    Work->OnStartedNative.AddLambda([&](APawn*){++Started;});
    Work->OnCancelledNative.AddLambda([&](APawn*){++Cancelled;});
    Work->OnCompletedNative.AddLambda([&](APawn*){++Completed;});
    TestTrue(TEXT("Hold starts"),Work->TryInteract(F.A,250));
    TestEqual(TEXT("Only marked action registers crime"),Activity->GetActiveCrimes().Num(),1);
    Work->CancelBy(F.A);
    TestTrue(TEXT("Cancel cleans before callbacks and never completes"),Activity->GetActiveCrimes().IsEmpty()&&Started==1&&Cancelled==1&&Completed==0);
    Work->TryInteract(F.A,250); Work->TickComponent(2.1f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Completion cleans registration once"),Activity->GetActiveCrimes().IsEmpty()&&Completed==1);
    Work->TryInteract(F.A,250); Activity->SetIncapacitated(true);
    TestTrue(TEXT("Down API cancels work and prevents new actions"),!Work->GetCurrentUser()&&Activity->GetActiveCrimes().IsEmpty()&&!Work->TryInteract(F.A,250));
    Activity->SetIncapacitated(false);
    Work->TryInteract(F.A,250); auto* PC=F.A->GetController(); PC->UnPossess();
    TestTrue(TEXT("Unpossess/disconnect clears live work"),!Work->GetCurrentUser()&&Activity->GetActiveCrimes().IsEmpty());
    PC->Possess(F.A); Work->TryInteract(F.A,250); F.S->RestartStage();
    TestTrue(TEXT("Stage restart cancels pending work"),!Work->GetCurrentUser()&&Activity->GetActiveCrimes().IsEmpty());
    Work->TryInteract(F.A,250); Work->GetOwner()->Destroy();
    TestTrue(TEXT("Target destruction clears registration"),Activity->GetActiveCrimes().IsEmpty());
    auto* Other=F.Interaction(); Other->TryInteract(F.A,250); F.A->Destroy();
    TestNull(TEXT("Pawn destruction releases target"),Other->GetCurrentUser());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPackingAndNormalTest,"SpacePirate.Stealth.Crime.PackingVersusNormal",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSPPackingAndNormalTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* G=F.World->SpawnActor<ASPGuardCharacter>(); G->SetActorLocation(FVector(0,0,96));
    TestFalse(TEXT("MVP footsteps default off"),G->bHearFootsteps);
    auto* Normal=F.Interaction(ESPCrimeKind::None,2);
    Normal->TryInteract(F.A,250); G->Tick(1.1f);
    TestTrue(TEXT("Unmarked E and normal public player stay legal"),!F.Exposed(F.A)&&F.A->GetStealthActivity()->GetActiveCrimes().IsEmpty());
    Normal->CancelBy(F.A);
    auto* Bag=F.World->SpawnActor<ASPCargo>(); Bag->SetActorLocation(F.A->GetActorLocation()+FVector(0,100,0));
    Bag->GetInteractable()->TryInteract(F.A,250); G->Tick(1.1f);
    TestTrue(TEXT("Floor pickup/carry is not a crime"),F.A->GetInventory()->GetSlots().Contains(Bag)&&!F.Exposed(F.A));
    auto* Bundle=F.World->SpawnActor<ASPLootBundle>(); Bundle->SetActorLocation(F.A->GetActorLocation()+FVector(80,0,0));
    TestTrue(TEXT("Existing loot bundle opts into packing"),Bundle->GetInteractable()->CrimeKind==ESPCrimeKind::LootPacking);
    Bundle->GetInteractable()->TryInteract(F.A,250); G->Tick(.6f);
    TestFalse(TEXT("Visible packing still needs one second"),F.Exposed(F.A));
    G->Tick(.5f);
    TestTrue(TEXT("Public-area packing exposes exactly the packer"),F.Exposed(F.A)&&!F.Exposed(F.B));
    Bundle->GetInteractable()->CancelBy(F.A);
    TestTrue(TEXT("Packing cancellation removes active crime"),F.A->GetStealthActivity()->GetActiveCrimes().IsEmpty());
    F.S->RestartStage();
    auto* Wall=F.Wall();
    auto C=F.S->MakeIncidentContext(F.A->GetActorLocation(),G->AlertGroup);
    F.S->ReportWorkNoise(C);
    TestTrue(TEXT("Occluded work noise investigates without identity/alarm"),G->bInvestigatingAnonymousIncident&&!G->TargetPlayer&&!F.Exposed(F.A)
        &&!F.S->GetSecurityState()->GetAlarmState().bGlobalAlarm&&!G->CanSeePlayer(F.A));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPCrimeKindsTest,"SpacePirate.Stealth.Crime.KindsAndReentrantStart",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSPCrimeKindsTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* Work=F.Interaction();
    const ESPCrimeKind Kinds[]={ESPCrimeKind::Pickpocket,ESPCrimeKind::SecurityTerminal,ESPCrimeKind::VaultWork,ESPCrimeKind::LootPacking,ESPCrimeKind::SuppressionTool};
    for (auto Kind:Kinds)
    {
        const auto Id=F.A->GetStealthActivity()->BeginCrime(Kind,Work);
        TestTrue(TEXT("Distinct crime kind is preserved"),Id.IsValid()&&F.A->GetStealthActivity()->GetActiveCrimes().Contains(Kind));
        F.A->GetStealthActivity()->EndCrime(Id);
    }
    int32 Cancels=0;
    Work->OnStartedNative.AddLambda([&](APawn* User){Work->CancelBy(User);});
    Work->OnCancelledNative.AddLambda([&](APawn* User){++Cancels; Work->TryInteract(User,250);});
    TestFalse(TEXT("Start callback cancellation does not leave an active hold"),Work->TryInteract(F.A,250));
    TestTrue(TEXT("Reentrant cancel callback cannot restart a transitioning interaction"),Cancels==1&&!Work->GetCurrentUser()&&F.A->GetStealthActivity()->GetActiveCrimes().IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPStageResetCallbackTest, "SpacePirate.Stealth.Crime.StageResetCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPStageResetCallbackTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* Work = F.Interaction();
    int32 AlarmEffects = 0;
    int32 CancelCount = 0;
    bool bActiveDuringCancel = true;
    FGuid NestedStage;
    ESPStealthResult DuringReset = ESPStealthResult::InvalidRequest;
    F.S->OnFirstGlobalAlarmNative.AddLambda([&](const FSPStealthIncidentRecord&) { ++AlarmEffects; });
    Work->OnCancelledNative.AddLambda([&](APawn*)
    {
        ++CancelCount;
        bActiveDuringCancel = F.S->IsStageActive();
        DuringReset = F.S->SubmitDirectIncident(ESPStealthIncident::DirectReportCompleted,
            F.A->GetPlayerState(), TEXT("StageSecurity"), F.S->MakeIncidentContext(FVector::ZeroVector, NAME_None)).Result;
        NestedStage = F.S->RestartStage();
    });
    TestTrue(TEXT("Work is active before restart"), Work->TryInteract(F.A, 250));
    const FGuid Stage = F.S->RestartStage();
    TestEqual(TEXT("Restart cancels once"), CancelCount, 1);
    TestFalse(TEXT("Producers cannot act while resetting"), bActiveDuringCancel);
    TestTrue(TEXT("Cancellation callbacks cannot raise a new incident during reset"), DuringReset == ESPStealthResult::InactiveStage);
    TestFalse(TEXT("Nested stage transition is rejected"), NestedStage.IsValid());
    TestTrue(TEXT("One consistent, active generation after reset"), Stage.IsValid() && F.S->IsStageActive()
        && F.S->GetSecurityState()->GetAlarmState().StageId == Stage);
    TestTrue(TEXT("Clean restart has no alarm or identity"), !F.S->GetSecurityState()->GetAlarmState().bGlobalAlarm
        && !F.Exposed(F.A) && !F.Exposed(F.B));
    TestEqual(TEXT("No first effects escape the reset"), AlarmEffects, 0);

    TestTrue(TEXT("Work can start after reset returns"), Work->TryInteract(F.A, 250));
    TestTrue(TEXT("End stage succeeds"), F.S->EndStage());
    TestTrue(TEXT("End cancellation cannot reactivate the stage"), !NestedStage.IsValid() && !F.S->IsStageActive()
        && DuringReset == ESPStealthResult::InactiveStage && CancelCount == 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGravityVisibilityTest, "SpacePirate.Stealth.Crime.GravityAlignedVisibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGravityVisibilityTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* Eye = F.Observer(FVector(0, 0, 96));
    Eye->Sight.HorizontalAngle = 4;
    Eye->Sight.VerticalAngle = 4;
    for (const float Roll : {0.0f, 90.0f, 180.0f})
    {
        F.A->SetActorRotation(FRotator(0, 0, Roll));
        const auto* Capsule = F.A->GetCapsuleComponent();
        const FVector UpperBody = Capsule->GetComponentLocation()
            + Capsule->GetUpVector() * Capsule->GetScaledCapsuleHalfHeight() * 0.65f;
        Eye->SetWorldRotation((UpperBody - Eye->GetComponentLocation()).Rotation());
        TestTrue(FString::Printf(TEXT("Actual upper body is visible with capsule roll %.0f"), Roll), Eye->CanSeePlayer(F.A));
        Eye->SetWorldRotation(FRotator(0, 180, 0));
        TestFalse(TEXT("Gravity rotation does not bypass the observer's view direction"), Eye->CanSeePlayer(F.A));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPHealthStealthIntegrationTest, "SpacePirate.Stealth.Crime.HealthDownAndRevive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPHealthStealthIntegrationTest::RunTest(const FString& Parameters)
{
    SPCrimeTests::FWorld F;
    auto* Work = F.Interaction();
    auto* Guard = F.World->SpawnActor<ASPGuardCharacter>();
    Guard->SetActorLocation(FVector(0, 0, 96));
    F.A->GetInteractor()->StartInteract(Work);
    Guard->Tick(1.01f);
    TestTrue(TEXT("Visible ongoing work identifies its player before down"), F.Exposed(F.A) && Guard->TargetPlayer == F.A);

    F.A->GetStatusComponent()->ApplyDamage(F.A->GetStatusComponent()->GetMaxHealth());
    TestTrue(TEXT("Health down immediately cancels stealth work"), F.A->IsDowned()
        && F.A->GetStealthActivity()->IsIncapacitated()
        && F.A->GetStealthActivity()->GetActiveCrimes().IsEmpty()
        && !Work->GetCurrentUser() && !F.A->GetInteractor()->IsHolding());
    TestFalse(TEXT("Downed player cannot restart work"), Work->TryInteract(F.A, 250));
    Guard->Tick(0.1f);
    TestTrue(TEXT("Downed player is not a direct pursuit or sight target"), !Guard->TargetPlayer && !Guard->CanSeePlayer(F.A));
    TestTrue(TEXT("Down does not erase identity or identify a teammate"), F.Exposed(F.A) && !F.Exposed(F.B));

    TestTrue(TEXT("Existing health owner revives player"), F.A->GetStatusComponent()->TryRevive(F.B));
    TestTrue(TEXT("Recovery re-enables actions while preserving identity"), !F.A->GetStealthActivity()->IsIncapacitated()
        && F.Exposed(F.A) && Work->TryInteract(F.A, 250));
    Work->CancelBy(F.A);
    TestTrue(TEXT("Rescue is a normal interaction, never an opt-in crime"), F.A->GetReviveInteraction()->CrimeKind == ESPCrimeKind::None);
    return true;
}
#endif
