#pragma once

// 역할: 입력과 로컬 카메라를 관리한다. 실제 이동/몸 회전/예측은 SPCharacterMovementComponent가 담당한다.
// NOTICE [TEMP-GRAVITY-SWITCH]: 정식 장치 도입 후 FindGravitySwitchInView/ServerUseGravitySwitch와
// Interact의 버튼 우선 분기를 교체한다. 기존 화물 상호작용과 영역 시스템은 유지한다.
// 작업자: 김세훈 (중력 영역/버튼 연동)

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SPGravityTypes.h"
#include "SPPlayerCharacter.generated.h"

class ASPCargo;
class ASPGravitySwitch;
class UCameraComponent;
class UInputAction;
class USceneComponent;
class USPInventoryComponent;
struct FInputActionValue;

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

    /** 애님 BP의 운반 포즈 전환용. 현재 칸에 화물이 있으면 true. 인벤토리가 복제되므로 다른 플레이어 화면에서도 맞다. */
    UFUNCTION(BlueprintPure, Category = "Cargo")
    bool IsCarryingCargo() const;

    USPInventoryComponent* GetInventory() const;

protected:
    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

    virtual void OnConstruction(const FTransform& Transform) override;

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

    /** 3칸 인벤토리. 서버 거리 검사와 버리기 여유 거리는 이 컴포넌트 Details에서 조절한다. */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<USPInventoryComponent> Inventory;

    /** Axis2D: X=좌우, Y=전후. 키 배치는 기존 Input Mapping Context에서 설정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction; // Space: 중력에서는 점프, 무중력에서는 몸 위쪽 추진.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SprintAction; // Shift: 중력에서는 달리기, 무중력에서는 몸 아래쪽 추진.

    // 인벤토리 입력. 비어 있으면 그 기능만 비활성화되고 다른 입력은 그대로 동작한다.
    /** E: 화면 중앙의 화물을 집는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InteractAction;

    /** G: 현재 칸의 화물을 버린다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> DropAction;

    /** Axis1D: 숫자키 1/2/3에 Scalar 모디파이어로 1, 2, 3을 넣는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SelectSlotAction;

    /** Axis1D: 마우스 휠. 양수면 다음 칸, 음수면 이전 칸. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> CycleSlotAction;

    /** 화면 중앙에서 화물을 찾는 구체 트레이스 길이. */
    UPROPERTY(EditDefaultsOnly, Category = "Cargo", meta = (ClampMin = "0.0"))
    float CargoTraceDistance = 250.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Cargo", meta = (ClampMin = "0.0"))
    float CargoTraceRadius = 20.0f;

    /** 화물을 찾는 트레이스 채널. 화물 메시가 이 채널을 Block해야 집을 수 있다. */
    UPROPERTY(EditDefaultsOnly, Category = "Cargo", AdvancedDisplay)
    TEnumAsByte<ECollisionChannel> CargoTraceChannel = ECC_Visibility;

    /** CargoHoldPoint가 붙을 메시 소켓(또는 본) 이름. 바꾸면 BP 뷰포트 미리보기에도 반영된다. */
    UPROPERTY(EditDefaultsOnly, Category = "Cargo")
    FName CargoHoldSocketName = TEXT("hand_r");

    /** 임시 중력 버튼 검색 거리(cm). 서버도 같은 거리와 가림 상태를 재검사한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gravity|Interaction", meta = (ClampMin = "1.0"))
    float GravitySwitchUseDistance = 250.0f;

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

    // 클라이언트는 화면 기준으로 대상만 고르고, 판정과 부착은 인벤토리 컴포넌트가 서버에서 한다.
    void Interact();
    void DropActiveCargo();
    void SelectSlot(const FInputActionValue& Value);
    void CycleSlot(const FInputActionValue& Value);
    ASPCargo* FindCargoInView() const;

    // 키를 누른 상태를 유지해 중력 전환 후에도 같은 입력을 새 모드에서 해석한다.
    FVector2D MoveInput = FVector2D::ZeroVector;
    bool bSprintHeld = false;
    // 같은 설정 오류가 입력 프레임마다 반복 출력되지 않도록 하는 플래그.
    bool bReportedMissingController = false;
    bool bReportedInvalidMovementComponent = false;

    void RefreshZeroGravityInput();
    void UpdateCameraForMovementMode();

    /** 작업자: 김세훈 | [TEMP-GRAVITY-SWITCH] 실제 시선에서 첫 번째 버튼을 찾는다. */
    ASPGravitySwitch* FindGravitySwitchInView() const;

    /** 작업자: 김세훈 | [TEMP-GRAVITY-SWITCH] 서버에서 거리/시선을 재검사하고 버튼을 사용한다. */
    UFUNCTION(Server, Reliable)
    void ServerUseGravitySwitch(ASPGravitySwitch* TargetSwitch);

    bool bUpThrustHeld = false;

    // 조절용 기본값이 아니라 무중력 진입 전 카메라 설정의 백업. 복귀 시 원래 값을 복원한다.
    bool bCachedCameraPitchLimits = false;
    float SavedCameraPitchMin = -70.0f;
    float SavedCameraPitchMax = 80.0f;
};
