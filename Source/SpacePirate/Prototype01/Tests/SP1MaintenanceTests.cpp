// 작성자 : 임진혁
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Prototype01/SP1RunRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1MaintenanceClockTest,"SpacePirate.Prototype01.MaintenanceClockAndBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSP1MaintenanceClockTest::RunTest(const FString&)
{
    FSP1RunState Run; Run.Phase=ESP1Phase::Running; Run.MissionDeadline=420;
    Run.Maintenance.Phase=ESP1MaintenancePhase::Active;
    Run.Maintenance.StartedAt=100; Run.Maintenance.Deadline=130; Run.Maintenance.FrozenMissionRemaining=320;
    TestEqual(TEXT("Only mission remaining freezes while real server time advances"),SP1::MissionRemaining(Run,129),320.);
    TestTrue(TEXT("Mid departure just before maintenance deadline is valid"),SP1::CanStartDeparture(Run,true,129.99));
    TestFalse(TEXT("Exact maintenance boundary rejects departure before mutation"),SP1::CanStartDeparture(Run,true,130));
    TestTrue(TEXT("Close once using scheduled boundary even on late frame"),SP1::CloseMaintenance(Run,130,TEXT("WindowExpired")));
    TestEqual(TEXT("Only thirty seconds added"),Run.MissionDeadline,450.);
    TestEqual(TEXT("Late frame consumes time after boundary"),SP1::MissionRemaining(Run,132),318.);
    TestFalse(TEXT("Reentry or second close cannot add free mission time"),SP1::CloseMaintenance(Run,160,TEXT("Repeated")));
    TestFalse(TEXT("Mid exit closed"),SP1::CanStartDeparture(Run,true,132));
    TestTrue(TEXT("Final exit still valid"),SP1::CanStartDeparture(Run,false,132));
    Run.DepartureId=TEXT("Final"); Run.Phase=ESP1Phase::ExtractionCountdown; Run.ExtractionDeadline=450;
    TestFalse(TEXT("Other exit cannot start in same RunId"),SP1::CanStartDeparture(Run,false,133));
    const auto End=SP1::ResolveEnd(Run.Phase,450,Run.MissionDeadline,Run.ExtractionDeadline,4,4);
    TestEqual(TEXT("Expiration still beats full extraction after maintenance"),End->Reason,FName(TEXT("MissionExpired")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1ReviveEligibilityTest,"SpacePirate.Prototype01.MaintenanceReviveOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSP1ReviveEligibilityTest::RunTest(const FString&)
{
    FSP1Participant P;
    TestFalse(TEXT("Living teammate is not healed"),SP1::CanRevive(P));
    P.bAlive=false;
    TestTrue(TEXT("Connected dead teammate can revive"),SP1::CanRevive(P));
    P.bConnected=false;
    TestFalse(TEXT("Disconnected dead teammate excluded"),SP1::CanRevive(P));
    P.bConnected=true; P.bMaintenanceRevived=true;
    TestFalse(TEXT("Second hold / death does not reset once-per-participant ledger"),SP1::CanRevive(P));
    P=FSP1Participant(); P.bAlive=false;
    TestTrue(TEXT("New run participant gets a fresh allowance"),SP1::CanRevive(P));
    return true;
}
#endif
