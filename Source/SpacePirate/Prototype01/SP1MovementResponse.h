// 작성자 : 임진혁
#pragma once
#include "GameFramework/CharacterMovementReplication.h"
#include "Prototype01/SP1SurvivalRules.h"

/** 서버가 승인한 이동 시각의 스태미나를 응답에 싣는다. 클라이언트에서 서버로 값을 보내지 않는다. */
struct FSP1MovementResponse : FCharacterMoveResponseDataContainer
{
    bool bHasSurvival = false;
    FSP1StaminaState Stamina;
    float Wounds = 0;
    virtual void ServerFillResponseData(const UCharacterMovementComponent& Movement, const FClientAdjustment& Adjustment) override;
    virtual bool Serialize(UCharacterMovementComponent& Movement, FArchive& Ar, UPackageMap* Map) override;
};
