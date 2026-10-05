// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
namespace SP1
{
    // 노출은 개인별 0~1 값이다. 이탈 시 '현재 값의 비율'이 아니라 초당 0.5를 뺀다.
    inline float UpdateExposure(float Value, bool bVisible, float Delta, float ExposureSeconds, float DecayPerSecond)
    {
        return FMath::Clamp(Value + FMath::Max(0.f, Delta) * (bVisible ? 1.f/FMath::Max(.1f,ExposureSeconds) : -FMath::Max(0.f,DecayPerSecond)),0.f,1.f);
    }
}
