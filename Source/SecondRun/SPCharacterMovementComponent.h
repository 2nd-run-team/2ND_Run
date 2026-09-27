#pragma once

// 역할: 중력/무중력 이동, 충돌 처리, 클라이언트 예측과 서버 보정을 담당한다.
// 튜닝: 플레이어 BP의 Character Movement 컴포넌트에서 기본값을 설정한다.
// 아래 값은 자동 복제되지 않는다. 서버와 클라이언트가 같은 BP 기본값을 사용해야 한다.
// BlueprintReadOnly는 그래프의 읽기만 허용한다는 뜻이며, Details의 기본값 편집은 가능하다.
// NOTICE [ZG-PROTOTYPE]: 현재는 키보드 추진과 Pitch/Yaw 추종만 지원한다(Roll 입력 없음).
// 이동 기록 병합 제한, 회전 충돌 샘플링의 한계는 이 클래스의 .cpp 상단을 참고한다.

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SPGravityTypes.h"
#include "SPMovementNetwork.h"
#include "SPCharacterMovementComponent.generated.h"

UCLASS()
class SECONDRUN_API USPCharacterMovementComponent
    : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    USPCharacterMovementComponent();

    // 엔진의 CustomMovementMode 식별자. 속도처럼 조절하는 튜닝 값이 아니다.
    static constexpr uint8 ZeroGravityCustomMode = 1;

    UFUNCTION(BlueprintPure, Category = "Movement|Gravity")
    bool IsZeroGravity() const
    {
        return MovementMode == MOVE_Custom
            && CustomMovementMode == ZeroGravityCustomMode;
    }

    UFUNCTION(BlueprintPure, Category = "Movement|Gravity")
    ESPGravityMode GetGravityMode() const
    {
        return IsZeroGravity()
            ? ESPGravityMode::ZeroGravity
            : ESPGravityMode::Gravity;
    }

    // 테스트 키뿐 아니라 추후 중력 구역에서도 호출할 진입점.
    // 실제 상태 변경은 서버만 수행한다.
    UFUNCTION(
        BlueprintCallable,
        BlueprintAuthorityOnly,
        Category = "Movement|Gravity")
    bool SetGravityMode(ESPGravityMode NewMode);

    // 몸 기준 X=전후, Y=좌우, Z=상하. 여기서는 저장만 하고 PhysCustom에서 추진한다.
    void SetZeroGravityInput(const FVector& Input);

    FVector GetZeroGravityInput() const
    {
        return LocalThrustInput;
    }

    void SetSprintRequested(bool bRequested);

    bool IsSprintRequested() const
    {
        return bSprintRequested;
    }

    virtual float GetMaxSpeed() const override;

    // 기본 위치 보정에 몸 회전도 포함한다.
    virtual bool ShouldCorrectRotation() const override
    {
        return true;
    }

    virtual FNetworkPredictionData_Client*
        GetPredictionData_Client() const override;

    virtual bool ClientUpdatePositionAfterServerUpdate() override;

    /** 지상 달리기 속도(cm/s). 걷기 속도보다 낮게 설정하면 걷기 속도를 사용한다. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Movement|Sprint",
        meta = (ClampMin = "0.0"))
    float SprintSpeed = 700.0f;

    /** 전방과 이동 입력의 내적 하한. 기본 0.5는 전진 대각선(약 0.707)을 허용한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Sprint", AdvancedDisplay,
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SprintForwardDotThreshold = 0.5f;

    /** 최대 추진 가속도(cm/s²). 대각선/3축 입력을 함께 눌러도 이 크기를 넘지 않는다. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Movement|Zero Gravity",
        meta = (ClampMin = "0.0"))
    float ZeroGravityAcceleration = 500.0f;

    /** 추진으로 도달하는 속도 상한(cm/s). 전환 전에 이미 더 빨랐다면 기존 속도를 보존한다. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Movement|Zero Gravity",
        meta = (ClampMin = "1.0"))
    float ZeroGravityMaxSpeed = 700.0f;

    /** 시선을 따라 몸이 회전하는 최대 각속도(도/초). 0이면 몸은 회전하지 않는다. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Movement|Zero Gravity",
        meta = (ClampMin = "0.0"))
    float ZeroGravityTurnRate = 90.0f;

    /** 속도 감쇠 계수(1/초). 0이면 관성을 유지하고, 양수이면 exp(-Drag * 시간)으로 감속한다. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Movement|Zero Gravity",
        meta = (ClampMin = "0.0"))
    float ZeroGravityDrag = 0.0f;

    /** 무중력 시선과 몸의 Pitch 하한(도). 카메라도 같은 값을 사용하며 -90도 뒤집힘은 제한한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Zero Gravity|View",
        meta = (ClampMin = "-89.0", ClampMax = "0.0"))
    float ZeroGravityPitchMin = -89.0f;

    /** 무중력 시선과 몸의 Pitch 상한(도). 중력 복귀 시 카메라는 기존 제한으로 돌아간다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Zero Gravity|View",
        meta = (ClampMin = "0.0", ClampMax = "89.0"))
    float ZeroGravityPitchMax = 89.0f;

    // 아래 Advanced 값은 조작감보다 시뮬레이션 정밀도/비용에 영향을 준다. 기본값 유지 권장.
    /** 무중력 계산을 나누는 시간 간격(초). 작을수록 회전 중 추진 계산이 세밀해지지만 비용이 증가한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Zero Gravity|Simulation", AdvancedDisplay,
        meta = (ClampMin = "0.001", ClampMax = "0.05"))
    float ZeroGravityMaxSimulationTimeStep = 1.0f / 120.0f;

    /** 한 이동에서 시간 분할 횟수 상한. 마지막 분할은 남은 시간을 모두 처리한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Zero Gravity|Simulation", AdvancedDisplay,
        meta = (ClampMin = "1", ClampMax = "128"))
    int32 ZeroGravityMaxSimulationIterations = 32;

    /** 시간 분할 하나에서 벽/모서리 충돌을 처리하는 최대 횟수. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Zero Gravity|Simulation", AdvancedDisplay,
        meta = (ClampMin = "1", ClampMax = "16"))
    int32 ZeroGravityMaxCollisionIterations = 4;

    /** 캡슐 회전 중 겹침을 검사하는 각도 간격(도). 작을수록 정밀하지만 연속 회전 Sweep은 아니다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Zero Gravity|Simulation", AdvancedDisplay,
        meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float RotationCollisionStepDegrees = 2.0f;

    /** 서버가 몸 회전 보정을 요청하는 오차(도). 지나치게 작으면 잦은 보정이 발생할 수 있다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement|Zero Gravity|Network", AdvancedDisplay,
        meta = (ClampMin = "0.1", ClampMax = "45.0"))
    float NetworkBodyRotationTolerance = 2.0f;

protected:
    virtual void BeginPlay() override;

    virtual void UpdateFromCompressedFlags(uint8 Flags) override;

    virtual void PhysCustom(
        float DeltaTime,
        int32 Iterations) override;

    virtual void MoveAutonomous(
        float ClientTimeStamp,
        float DeltaTime,
        uint8 CompressedFlags,
        const FVector& NewAccel) override;

    virtual bool ServerCheckClientError(
        float ClientTimeStamp,
        float DeltaTime,
        const FVector& Accel,
        const FVector& ClientWorldLocation,
        const FVector& RelativeClientLocation,
        FMovementBaseInterfaceData* ClientMovementBaseInterfaceData,
        FName ClientBaseBoneName,
        uint8 ClientMovementMode) override;

private:
    bool HasForwardAcceleration() const;

    FRotator GetSimulationViewRotation() const;

    bool CanOccupyRotation(const FQuat& Rotation) const;
    bool TrySetBodyRotation(const FQuat& TargetRotation);

    void RotateZeroGravityBody(float DeltaTime);
    void MoveZeroGravity(float DeltaTime);

    bool bSprintRequested = false;
    FVector LocalThrustInput = FVector::ZeroVector;

    // 엔진이 포인터로 참조하므로 지역 변수가 아닌 컴포넌트 수명의 멤버로 보관한다.
    FSPNetworkMoveDataContainer SPNetworkMoveDataContainer;
};
