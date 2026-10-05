#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SPCrouchAnimInstance.generated.h"

/** 기존 이동/운반 애니메이션 뒤에 앉기 자세만 합성하는 시험용 후처리 BP의 부모. */
UCLASS(Transient, Blueprintable)
class SPACEPIRATE_API USPCrouchAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category = "Stealth")
    float CrouchAlpha = 0.0f;
};
