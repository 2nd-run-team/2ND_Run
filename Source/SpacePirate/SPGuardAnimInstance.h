#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SPGuardAnimInstance.generated.h"

/** AI와 원격 프록시 모두 입력 가속도가 아닌 실제 이동속도로 보행을 재생한다. */
UCLASS()
class SPACEPIRATE_API USPGuardAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation")
    float GroundSpeed = 0;

    /** 공용 Manny 블렌드 스페이스의 걷기 300 / 달리기 600 좌표. */
    UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation")
    float BlendSpeed = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation")
    float Direction = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Guard|Animation")
    float StridePlayRate = 1;
};
