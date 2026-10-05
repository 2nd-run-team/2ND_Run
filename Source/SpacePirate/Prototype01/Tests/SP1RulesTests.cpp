// 작성자 : 임진혁
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Prototype01/SP1RunRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1SettlementTest, "SpacePirate.Prototype01.SettlementOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSP1SettlementTest::RunTest(const FString& Parameters)
{
    FSP1RunLedger Ledger;
    Ledger.RunId = FGuid::NewGuid();
    const FGuid Run = Ledger.RunId;
    TestTrue(TEXT("First player transfers cargo"), Ledger.Credit(Run, TEXT("C01_A"), 100));
    TestFalse(TEXT("Concurrent player cannot add the same cargo"), Ledger.Credit(Run, TEXT("C01_A"), 100));
    TestFalse(TEXT("Previous run cannot add value"), Ledger.Credit(FGuid::NewGuid(), TEXT("C01_B"), 500));
    TestTrue(TEXT("Other cargo contributes independently"), Ledger.Credit(Run, TEXT("C01_B"), 250));
    TestTrue(TEXT("Result first accepted"), Ledger.Finalize(Run, ESP1Phase::Succeeded));
    TestFalse(TEXT("Repeated result cannot overwrite"), Ledger.Finalize(Run, ESP1Phase::Failed));
    TestFalse(TEXT("Late completion after result cannot add score"), Ledger.Credit(Run, TEXT("C01_C"), 500));
    TestEqual(TEXT("One result includes one credit per cargo"), Ledger.FinalValue, 350);
    FSP1RunLedger Failed;
    Failed.RunId = FGuid::NewGuid();
    Failed.Credit(Failed.RunId, TEXT("C01_A"), 100);
    Failed.Finalize(Failed.RunId, ESP1Phase::Failed);
    TestEqual(TEXT("Failed run preserves unsettled value"), Failed.TeamValue, 100);
    TestEqual(TEXT("Failed run pays nothing"), Failed.FinalValue, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1AttemptIdentityTest, "SpacePirate.Prototype01.AttemptIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSP1AttemptIdentityTest::RunTest(const FString& Parameters)
{
    FSP1Attempt Active;
    Active.RunId = FGuid::NewGuid();
    Active.RequestId = 8;
    Active.AttemptId = 42;
    Active.bActive = true;
    TestFalse(TEXT("Old release does not erase new hold"), SP1::MatchesAttempt(Active, Active.RunId, 7, 41));
    TestFalse(TEXT("Old ACK does not match new attempt"), SP1::MatchesAttempt(Active, Active.RunId, 8, 41));
    TestFalse(TEXT("Old run is rejected even with same sequence"), SP1::MatchesAttempt(Active, FGuid::NewGuid(), 8, 42));
    TestTrue(TEXT("Release before ACK still cancels its own request"), SP1::MatchesAttempt(Active, Active.RunId, 8, 0));
    Active.bActive = false;
    TestFalse(TEXT("Late heartbeat cannot revive canceled hold"), SP1::MatchesAttempt(Active, Active.RunId, 8, 42));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1EndOrderingTest, "SpacePirate.Prototype01.EndOrdering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSP1EndOrderingTest::RunTest(const FString& Parameters)
{
    // 남은 9초에서 10초 출발을 시작하면 탑승자가 있어도 지급하지 않는다.
    auto End = SP1::ResolveEnd(ESP1Phase::ExtractionCountdown, 9, 9, 10, 2, 2);
    TestTrue(TEXT("Mission expiration resolves before departure"), End.IsSet());
    TestEqual(TEXT("Deadline wins"), End->Reason, FName(TEXT("MissionExpired")));
    End = SP1::ResolveEnd(ESP1Phase::ExtractionCountdown, 10, 10, 10, 2, 2);
    TestEqual(TEXT("Exact tie also fails"), End->Phase, ESP1Phase::Failed);
    End = SP1::ResolveEnd(ESP1Phase::ExtractionCountdown, 10, 11, 10, 2, 1);
    TestEqual(TEXT("One survivor inside before deadline succeeds"), End->Phase, ESP1Phase::Succeeded);
    End = SP1::ResolveEnd(ESP1Phase::ExtractionCountdown, 10, 11, 10, 2, 0);
    TestEqual(TEXT("Everyone left the zone"), End->Reason, FName(TEXT("EmptyExtraction")));
    End = SP1::ResolveEnd(ESP1Phase::ExtractionCountdown, 10, 11, 10, 0, 1);
    TestEqual(TEXT("No living connected player cannot extract"), End->Reason, FName(TEXT("NoConnectedSurvivors")));
    TestFalse(TEXT("Before either timer expires wait"), SP1::ResolveEnd(ESP1Phase::ExtractionCountdown, 8, 9, 10, 2, 2).IsSet());
    TestFalse(TEXT("Terminal state ignores late events"), SP1::ResolveEnd(ESP1Phase::Succeeded, 12, 9, 10, 2, 0).IsSet());
    return true;
}
#endif
