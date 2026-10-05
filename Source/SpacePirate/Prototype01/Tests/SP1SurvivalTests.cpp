// 작성자 : 임진혁
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Prototype01/SP1SurvivalRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1SurvivalRulesTest,"SpacePirate.Prototype01.SurvivalBudget", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSP1SurvivalRulesTest::RunTest(const FString&)
{
    FSP1StaminaState S;
    for (int32 I=0; I<100; ++I) SP1::StepStamina(S,0,0.05f,true,true);
    TestEqual(TEXT("Five seconds consumes 100 stamina"),S.Current,0.0f);
    TestFalse(TEXT("Exhaustion stops sprinting"),SP1::CanSprint(S));
    TestEqual(TEXT("Sprint did not wound player"),SP1::StaminaMaximum(0),100.0f);
    SP1::StepStamina(S,0,0.5f,false,false);
    TestEqual(TEXT("No recovery before delay"),S.Current,0.0f);
    SP1::StepStamina(S,0,0.5f,false,false);
    TestEqual(TEXT("Only 0.25 seconds recovers after the 0.75 delay"),S.Current,6.25f);
    SP1::StepStamina(S,80,10,false,false);
    TestEqual(TEXT("Wounded capacity caps regeneration"),S.Current,20.0f);
    SP1::StepStamina(S,100,1,true,true);
    TestEqual(TEXT("Death has no stamina"),S.Current,0.0f);
    TestFalse(TEXT("Death cannot sprint"),SP1::CanSprint(S));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1LaserClockTest,"SpacePirate.Prototype01.LaserClock", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSP1LaserClockTest::RunTest(const FString&)
{
    TestEqual(TEXT("Cycle starts off"),SP1::LaserPhase(100,100,2,2,.5),ESP1LaserPhase::Off);
    TestEqual(TEXT("Warning before on"),SP1::LaserPhase(101.5,100,2,2,.5),ESP1LaserPhase::Warning);
    TestEqual(TEXT("Warning lasts half second"),SP1::LaserPhase(101.99,100,2,2,.5),ESP1LaserPhase::Warning);
    TestEqual(TEXT("Exact on edge"),SP1::LaserPhase(102,100,2,2,.5),ESP1LaserPhase::On);
    TestEqual(TEXT("Exact off edge"),SP1::LaserPhase(104,100,2,2,.5),ESP1LaserPhase::Off);
    TestEqual(TEXT("Late client derives same cycle"),SP1::LaserPhase(109.5,100,2,2,.5),ESP1LaserPhase::Warning);
    return true;
}
#endif
