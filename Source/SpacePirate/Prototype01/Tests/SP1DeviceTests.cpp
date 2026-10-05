// 작성자 : 임진혁
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Prototype01/SP1DeviceRules.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSP1ExposureTest,"SpacePirate.Prototype01.IndividualExposureDecay",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSP1ExposureTest::RunTest(const FString&)
{
    float A = 0, B = 0;
    for (int32 I=0; I<15; ++I) { A=SP1::UpdateExposure(A,true,.1f,1.5f,.5f); B=SP1::UpdateExposure(B,false,.1f,1.5f,.5f); }
    TestEqual(TEXT("One player reaches detection after 1.5s"),A,1.f);
    TestEqual(TEXT("Exposure is not pooled across players"),B,0.f);
    TestEqual(TEXT("One second outside subtracts 50 percentage points"),SP1::UpdateExposure(.8f,false,1.f,1.5f,.5f),.3f);
    TestEqual(TEXT("Decay clamps at zero"),SP1::UpdateExposure(.2f,false,1.f,1.5f,.5f),0.f);
    return true;
}
#endif
