// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "SP1SurvivalRules.generated.h"

/** 이동 시뮬레이션/서버 응답에 같이 보관하는 스태미나 상태. 상처는 서버만 바꾼다. */
USTRUCT(BlueprintType)
struct FSP1StaminaState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) float Current = 100;
    UPROPERTY(BlueprintReadOnly) float RecoveryWait = 0;
    UPROPERTY(BlueprintReadOnly) bool bExhausted = false;
};

UENUM(BlueprintType)
enum class ESP1LaserPhase : uint8 { Off, Warning, On };

namespace SP1
{
    inline float StaminaMaximum(float Wounds) { return FMath::Max(0.0f, 100.0f - Wounds); }
    inline bool CanSprint(const FSP1StaminaState& State) { return State.Current > 0 && !State.bExhausted; }

    inline void StepStamina(FSP1StaminaState& State, float Wounds, float Delta, bool bEligible,
        bool bSprintKey, float Drain = 20, float Delay = 0.75f, float Recovery = 25)
    {
        const float Max = StaminaMaximum(Wounds);
        State.Current = FMath::Clamp(State.Current, 0.0f, Max);
        if (Max <= 0) { State = FSP1StaminaState{0, 0, true}; return; }
        if (!bSprintKey) State.bExhausted = false;
        if (bEligible && CanSprint(State))
        {
            State.Current = FMath::Max(0.0f, State.Current - FMath::Max(0.0f, Drain) * Delta);
            State.RecoveryWait = FMath::Max(0.0f, Delay);
            // 소진 뒤 키를 놓기 전에는 자동 재달리기를 막는다. 회복과 걷기는 계속 허용한다.
            if (State.Current <= KINDA_SMALL_NUMBER) { State.Current = 0; State.bExhausted = true; }
        }
        else
        {
            const float RecoverDelta = FMath::Max(0.0f, Delta - State.RecoveryWait);
            State.RecoveryWait = FMath::Max(0.0f, State.RecoveryWait - Delta);
            State.Current = FMath::Min(Max, State.Current + FMath::Max(0.0f, Recovery) * RecoverDelta);
        }
    }

    // 꺼짐 2초 안의 마지막 0.5초가 예고다. 예고를 더해 4.5초 주기로 늘리지 않는다.
    inline ESP1LaserPhase LaserPhase(double Time, double Epoch, float On, float Off, float Warning)
    {
        if (Time < Epoch) return ESP1LaserPhase::Off;
        On = FMath::Max(0.05f, On); Off = FMath::Max(0.05f, Off);
        const double PhaseTime = FMath::Fmod(Time - Epoch, static_cast<double>(On + Off));
        if (PhaseTime >= Off) return ESP1LaserPhase::On;
        return PhaseTime >= Off - FMath::Clamp(Warning, 0.0f, Off) ? ESP1LaserPhase::Warning : ESP1LaserPhase::Off;
    }
}
