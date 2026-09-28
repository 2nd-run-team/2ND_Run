#pragma once

// 역할: 입력과 로컬 카메라를 관리한다. 실제 이동/몸 회전/예측은 SPCharacterMovementComponent가 담당한다.
// NOTICE [TEMP-GRAVITY-TOGGLE]: '=' 키 중력 전환은 개발용 임시 기능이다.
// 정식 중력 구역 도입 후 이 헤더의 DebugToggleGravity/ServerSetDebugGravityMode 선언과
// .cpp의 키 바인딩 및 두 함수 구현을 함께 삭제한다. 이동 컴포넌트의 SetGravityMode는 유지한다.
// Shipping/Test에서는 키 바인딩과 함수 내부 동작을 제외한다(RPC 선언/빈 함수 자체는 남는다).

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SPGravityTypes.h"
#include "SPPlayerCharacter.generated.h"

class ASPCargo;
class UCameraComponent;
class UInputAction;
class USceneComponent;
struct FInputActionValue;
struct FKey;

UCLASS()
class SPACEPIRATE_API ASPPlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASPPlayerCharacter(
        const FObjectInitializer& ObjectInitializer =
        FObjectInitializer::Get());

    virtual void FaceRotation(
        FRotator NewControlRotation,
        float DeltaTime = 0.0f) override;

    virtual void OnMovementModeChanged(
        EMovementMode PrevMovementMode,
        uint8 PreviousCustomMode = 0) override;

    virtual void PawnClientRestart() override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 애님 BP의 운반 포즈 전환용. HeldCargo가 복제되므로 다른 플레이어 화면에서도 맞다. */
    UFUNCTION(BlueprintPure, Category = "Cargo")
    bool IsCarryingCargo() const;

protected:
    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

    /** 캡슐에 부착된 1인칭 카메라. 위치/FOV는 BP 컴포넌트 Details에서 조절한다. */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    /**
     * 든 화물이 붙는 위치. 메시의 hand_r 소켓에 붙어 운반 포즈의 손을 따라간다.
     * 손 기준 위치/회전은 BP 컴포넌트 Details에서 맞춘다.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<USceneComponent> CargoHoldPoint;

    /** Axis2D: X=좌우, Y=전후. 키 배치는 기존 Input Mapping Context에서 설정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction; // Space: 중력에서는 점프, 무중력에서는 몸 위쪽 추진.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SprintAction; // Shift: 중력에서는 달리기, 무중력에서는 몸 아래쪽 추진.

    /** 비어 있으면 상호작용만 비활성화되고 다른 입력은 그대로 동작한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InteractAction;

    /** 화면 중앙에서 화물을 찾는 구체 트레이스 길이. */
    UPROPERTY(EditDefaultsOnly, Category = "Cargo", meta = (ClampMin = "0.0"))
    float CargoTraceDistance = 250.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Cargo", meta = (ClampMin = "0.0"))
    float CargoTraceRadius = 20.0f;

    /** 서버가 허용하는 캐릭터-화물 최대 거리. 지연과 액터 원점 차이를 감안해 트레이스 길이보다 크게 둔다. */
    UPROPERTY(EditDefaultsOnly, Category = "Cargo", meta = (ClampMin = "0.0"))
    float CargoServerPickupRange = 400.0f;

    /** 마우스 입력 배율. 이동 컴포넌트의 몸 회전 속도와 별개로 시선 반응을 조절한다. */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Input",
        meta = (ClampMin = "0.01"))
    float MouseSensitivity = 1.0f;

    /** 달리기를 요청할 최소 전진 입력. 실제 이동 방향은 이동 컴포넌트가 서버에서도 다시 검사한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", AdvancedDisplay,
        meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float SprintForwardInputThreshold = 0.1f;

private:
    void Move(const FInputActionValue& Value);
    void StopMove(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void StartJump();
    void EndJump();

    void StartSprint();
    void StopSprint();

    void UpdateSprintRequest();

    // 클라이언트는 화면 기준으로 대상만 고르고, 판정과 부착은 서버가 한다.
    void Interact();
    ASPCargo* FindCargoInView() const;

    UFUNCTION(Server, Reliable)
    void ServerRequestPickup(ASPCargo* TargetCargo);

    UPROPERTY(Replicated)
    TObjectPtr<ASPCargo> HeldCargo;

    // 키를 누른 상태를 유지해 중력 전환 후에도 같은 입력을 새 모드에서 해석한다.
    FVector2D MoveInput = FVector2D::ZeroVector;
    bool bSprintHeld = false;
    // 같은 설정 오류가 입력 프레임마다 반복 출력되지 않도록 하는 플래그.
    bool bReportedMissingController = false;
    bool bReportedInvalidMovementComponent = false;

    void RefreshZeroGravityInput();
    void UpdateCameraForMovementMode();

    // [TEMP-GRAVITY-TOGGLE] 제거 대상. 로컬 입력 -> 소유 캐릭터 RPC -> 서버 모드 변경.
    void DebugToggleGravity(FKey Key, FInputActionValue ActionValue);

    UFUNCTION(Server, Reliable)
    void ServerSetDebugGravityMode(ESPGravityMode NewMode);

    bool bUpThrustHeld = false;

    // 조절용 기본값이 아니라 무중력 진입 전 카메라 설정의 백업. 복귀 시 원래 값을 복원한다.
    bool bCachedCameraPitchLimits = false;
    float SavedCameraPitchMin = -70.0f;
    float SavedCameraPitchMax = 80.0f;
};
