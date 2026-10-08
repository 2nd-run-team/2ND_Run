#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SPGuardAlertSubsystem.h"
#include "SPGuardCharacter.h"
#include "SPStealthGameStateComponent.h"
#include "SPStealthPlayerStateComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace SPStealthTests
{
    struct FWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        USPGuardAlertSubsystem* S = nullptr;
        APlayerState* A = nullptr;
        APlayerState* B = nullptr;
        FWorld()
        {
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            World->GetWorldSettings()->NotifyBeginPlay();
            if (!World->GetGameState()) { World->SetGameState(World->SpawnActor<AGameStateBase>()); }
            S = World->GetSubsystem<USPGuardAlertSubsystem>();
            S->StartStage();
            A = World->SpawnActor<APlayerState>(); B = World->SpawnActor<APlayerState>();
            S->RegisterPlayer(A); S->RegisterPlayer(B);
        }
        ~FWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
        FSPStealthIncidentContext Context(FName Group = NAME_None) const { return S->MakeIncidentContext(FVector(400, 50, 90), Group); }
        bool Alarm() const { return S->GetSecurityState()->GetAlarmState().bGlobalAlarm; }
        bool Exposed(APlayerState* PS) const { return PS->FindComponentByClass<USPStealthPlayerStateComponent>()->IsIdentified(); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPIncidentPolicyTest, "SpacePirate.Stealth.Incidents.PolicyMatrix",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPIncidentPolicyTest::RunTest(const FString& Parameters)
{
    SPStealthTests::FWorld F;
    const ESPStealthIncident Anonymous[] = {ESPStealthIncident::LaserContact, ESPStealthIncident::WorkNoise,
        ESPStealthIncident::IndirectReport, ESPStealthIncident::VictimReport, ESPStealthIncident::EscapeActivated};
    for (auto Kind : Anonymous)
    {
        F.S->RestartStage();
        const auto Context = F.Context();
        const auto R = F.S->SubmitAnonymousIncident(Kind, Context);
        TestTrue(TEXT("Anonymous policy accepted"), R.Result == ESPStealthResult::Applied);
        TestTrue(TEXT("Receipt contains ID, kind, location and server clock"), R.Context.IncidentId == Context.IncidentId
            && R.Kind == Kind && R.Context.Location.Equals(Context.Location) && R.ServerTime == F.World->GetGameState()->GetServerWorldTimeSeconds());
        const bool bExpectedAlarm = Kind == ESPStealthIncident::VictimReport || Kind == ESPStealthIncident::EscapeActivated;
        TestEqual(TEXT("Only victim/escape raise anonymous alarm"), F.Alarm(), bExpectedAlarm);
        TestFalse(TEXT("Anonymous has no culprit"), R.Player.IsValid());
        TestFalse(TEXT("A remains hidden"), F.Exposed(F.A));
        TestFalse(TEXT("B remains hidden"), F.Exposed(F.B));
    }
    const ESPStealthIncident Direct[] = {ESPStealthIncident::SustainedCrimeConfirmed,
        ESPStealthIncident::InstantCrimeWitnessed, ESPStealthIncident::DirectReportCompleted};
    for (auto Kind : Direct)
    {
        F.S->RestartStage();
        const auto R = F.S->SubmitDirectIncident(Kind, F.A, TEXT("Network"), F.Context());
        TestTrue(TEXT("Direct confirmation identifies exactly A and raises alarm"), R.bFirstIdentification && R.bFirstGlobalAlarm
            && F.S->IsIdentified(TEXT("Network"), F.A) && F.Exposed(F.A) && !F.Exposed(F.B) && F.Alarm());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPIncidentIdempotencyTest, "SpacePirate.Stealth.Incidents.FirstEffectsAndRetries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPIncidentIdempotencyTest::RunTest(const FString& Parameters)
{
    SPStealthTests::FWorld F;
    int32 IdentityEffects = 0, AlarmEffects = 0;
    ESPStealthResult Reentrant = ESPStealthResult::InvalidRequest;
    const auto Context = F.Context();
    F.S->OnFirstPlayerIdentifiedNative.AddLambda([&](const FSPStealthIncidentRecord&)
    {
        ++IdentityEffects;
        Reentrant = F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed, F.A, TEXT("Network"), Context).Result;
    });
    F.S->OnFirstGlobalAlarmNative.AddLambda([&](const FSPStealthIncidentRecord&) { ++AlarmEffects; });
    auto R = F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed, F.A, TEXT("Network"), Context);
    TestTrue(TEXT("Reentrant retry already reserved"), Reentrant == ESPStealthResult::Duplicate);
    R = F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed, F.A, TEXT("Network"), Context);
    TestTrue(TEXT("Duplicate receipt has no repeated effects/dispatch"), R.Result == ESPStealthResult::Duplicate
        && !R.bFirstIdentification && !R.bFirstGlobalAlarm && R.DispatchedGuards == 0);
    R = F.S->SubmitDirectIncident(ESPStealthIncident::DirectReportCompleted, F.A, TEXT("OtherNetwork"), F.Context());
    TestTrue(TEXT("New scope shares identity without another first effect"), R.bNewIdentityScope && !R.bFirstIdentification);
    F.S->SubmitDirectIncident(ESPStealthIncident::SustainedCrimeConfirmed, F.B, TEXT("Network"), F.Context());
    F.S->SubmitAnonymousIncident(ESPStealthIncident::EscapeActivated, F.Context());
    TestEqual(TEXT("One first identity effect per player"), IdentityEffects, 2);
    TestEqual(TEXT("One global alarm effect per stage"), AlarmEffects, 1);
    // B's callback retries A's original ID; this must still be duplicate after many later receipts.
    for (int32 I = 0; I < 140; ++I) { F.S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise, F.Context()); }
    R = F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed, F.A, TEXT("Network"), Context);
    TestTrue(TEXT("Dedup survives recent-audit ring eviction"), R.Result == ESPStealthResult::Duplicate);
    TestEqual(TEXT("Recent history bounded, full history remains in log"), F.S->GetRecentIncidents().Num(), 128);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPIncidentScopeTest, "SpacePirate.Stealth.Incidents.KnowledgeVersusDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPIncidentScopeTest::RunTest(const FString& Parameters)
{
    SPStealthTests::FWorld F;
    auto* PC = F.World->SpawnActor<APlayerController>(); PC->PlayerState = F.A;
    auto* Pawn = F.World->SpawnActor<ASPPlayerCharacter>(); PC->Possess(Pawn);
    auto* Local = F.World->SpawnActor<ASPGuardCharacter>();
    auto* Remote = F.World->SpawnActor<ASPGuardCharacter>();
    auto* Outsider = F.World->SpawnActor<ASPGuardCharacter>();
    Local->AlertGroup = Outsider->AlertGroup = TEXT("Car1"); Remote->AlertGroup = TEXT("Car2");
    Local->IdentityScope = Remote->IdentityScope = TEXT("Network"); Outsider->IdentityScope = TEXT("OtherNetwork");
    F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed, F.A, TEXT("Network"), F.Context(TEXT("Car1")));
    TestTrue(TEXT("Dispatched guard in sharing scope pursues A"), Local->TargetPlayer == Pawn);
    TestTrue(TEXT("Remote group knows A"), F.S->IsIdentified(Remote->IdentityScope, F.A));
    TestNull(TEXT("Knowledge does not dispatch remote group"), Remote->TargetPlayer.Get());
    TestFalse(TEXT("Other identity scope does not learn A"), F.S->IsIdentified(Outsider->IdentityScope, F.A));
    TestTrue(TEXT("Dispatched outsider investigates without a target"), Outsider->bInvestigatingAnonymousIncident && !Outsider->TargetPlayer);
    auto R = F.S->SubmitDirectIncident(ESPStealthIncident::IdentifiedPlayerRediscovered, F.A, TEXT("Network"), F.Context(TEXT("Car2")));
    TestTrue(TEXT("Rediscovery dispatches selected group without first effects"), Remote->TargetPlayer == Pawn && !R.bFirstIdentification && !R.bFirstGlobalAlarm);
    R = F.S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise, F.Context(TEXT("Car2")));
    TestTrue(TEXT("Anonymous does not replace active pursuit"), Remote->TargetPlayer == Pawn && !Remote->bInvestigatingAnonymousIncident && R.DispatchedGuards == 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPIncidentLifecycleTest, "SpacePirate.Stealth.Incidents.StageLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPIncidentLifecycleTest::RunTest(const FString& Parameters)
{
    SPStealthTests::FWorld F;
    auto* Guard = F.World->SpawnActor<ASPGuardCharacter>();
    const auto Old = F.Context(Guard->AlertGroup);
    F.S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise, Old);
    TestTrue(TEXT("Anonymous location dispatched"), Guard->bInvestigatingAnonymousIncident && Guard->InvestigationLocation.Equals(Old.Location));
    F.S->SubmitDirectIncident(ESPStealthIncident::DirectReportCompleted, F.A, Guard->IdentityScope, F.Context());
    TestTrue(TEXT("Ending stage succeeds"), F.S->EndStage());
    TestTrue(TEXT("End clears alarm, identities and pending AI response"), !F.Alarm() && !F.Exposed(F.A)
        && !Guard->bInvestigatingAnonymousIncident && Guard->SuspicionProgress == 0 && !F.S->IsStageActive());
    auto R = F.S->SubmitAnonymousIncident(ESPStealthIncident::VictimReport, Old);
    TestTrue(TEXT("Ended stage rejects incoming completion"), R.Result == ESPStealthResult::InactiveStage && !F.Alarm());
    const auto NewStage = F.S->RestartStage();
    TestTrue(TEXT("Restart has new generation"), NewStage.IsValid() && NewStage != Old.StageId);
    R = F.S->SubmitAnonymousIncident(ESPStealthIncident::VictimReport, Old);
    TestTrue(TEXT("Delayed old-stage report rejected"), R.Result == ESPStealthResult::StaleStage && !F.Alarm());
    R = F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed, F.A, Guard->IdentityScope, F.Context());
    TestTrue(TEXT("First effects available again in new stage"), R.bFirstIdentification && R.bFirstGlobalAlarm);
    auto* Component = F.S->RegisterPlayer(F.A);
    TestTrue(TEXT("Register existing player preserves identity, no duplicate component"), Component == F.S->RegisterPlayer(F.A) && F.Exposed(F.A));
    auto* Late = F.World->SpawnActor<APlayerState>();
    TestTrue(TEXT("Late join starts hidden in current generation"), F.S->RegisterPlayer(Late)->GetIdentityState().StageId == NewStage && !F.Exposed(Late));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPIncidentValidationTest, "SpacePirate.Stealth.Incidents.InvalidRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPIncidentValidationTest::RunTest(const FString& Parameters)
{
    SPStealthTests::FWorld F;
    auto R = F.S->SubmitAnonymousIncident(ESPStealthIncident::InstantCrimeWitnessed, F.Context());
    TestTrue(TEXT("Anonymous API rejects direct kind"), R.Result == ESPStealthResult::InvalidRequest);
    R = F.S->SubmitDirectIncident(ESPStealthIncident::LaserContact, F.A, TEXT("Network"), F.Context());
    TestTrue(TEXT("Direct API cannot identify laser contact"), R.Result == ESPStealthResult::InvalidRequest);
    R = F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed, nullptr, TEXT("Network"), F.Context());
    TestTrue(TEXT("Direct API requires valid actor identity"), R.Result == ESPStealthResult::InvalidRequest);
    R = F.S->SubmitDirectIncident(ESPStealthIncident::IdentifiedPlayerRediscovered, F.A, TEXT("Network"), F.Context());
    TestTrue(TEXT("Rediscovery cannot identify an unknown player"), R.Result == ESPStealthResult::UnknownIdentity);
    auto Context = F.Context(); Context.IncidentId.Invalidate();
    R = F.S->SubmitAnonymousIncident(ESPStealthIncident::EscapeActivated, Context);
    TestTrue(TEXT("Missing ID rejected"), R.Result == ESPStealthResult::InvalidRequest);
    TestTrue(TEXT("Invalid requests have no gameplay state effect"), !F.Alarm() && !F.Exposed(F.A) && !F.Exposed(F.B));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPDispatchSelectionTest, "SpacePirate.Stealth.Incidents.DispatchSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPDispatchSelectionTest::RunTest(const FString& Parameters)
{
    SPStealthTests::FWorld F;
    auto* Near = F.World->SpawnActor<ASPGuardCharacter>();
    auto* Far = F.World->SpawnActor<ASPGuardCharacter>();
    auto* Other = F.World->SpawnActor<ASPGuardCharacter>();
    Near->SetActorLocation(FVector(300,0,96)); Far->SetActorLocation(FVector(800,0,96)); Other->SetActorLocation(FVector(100,0,96));
    Other->AlertGroup = TEXT("Other");
    auto C = F.S->MakeIncidentContext(FVector::ZeroVector, Near->AlertGroup);
    C.MaxResponders = 1; C.ResponseRadius = 1000;
    auto R = F.S->SubmitAnonymousIncident(ESPStealthIncident::LaserContact,C);
    TestTrue(TEXT("Nearest matching group, limited to one responder"),R.DispatchedGuards == 1 && Near->bInvestigatingAnonymousIncident
        && !Far->bInvestigatingAnonymousIncident && !Other->bInvestigatingAnonymousIncident);
    C.IncidentId = FGuid::NewGuid(); C.MaxResponders = 4; C.ResponseRadius = 500;
    R = F.S->SubmitDirectIncident(ESPStealthIncident::InstantCrimeWitnessed,F.A,Far->IdentityScope,C);
    TestTrue(TEXT("Radius excludes far response, never excludes identity knowledge"),R.DispatchedGuards == 1
        && !Far->bInvestigatingAnonymousIncident && F.S->IsIdentified(Far->IdentityScope,F.A));
    C.IncidentId = FGuid::NewGuid(); C.MaxResponders = 0;
    TestEqual(TEXT("Zero cap explicitly disables dispatch"),F.S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise,C).DispatchedGuards,0);
    C.IncidentId = FGuid::NewGuid(); C.ResponseRadius = -1;
    TestTrue(TEXT("Invalid radius rejected"),F.S->SubmitAnonymousIncident(ESPStealthIncident::WorkNoise,C).Result == ESPStealthResult::InvalidRequest);
    return true;
}
#endif

