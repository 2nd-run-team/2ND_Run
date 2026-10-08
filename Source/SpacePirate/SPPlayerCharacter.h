#pragma once

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 상태·구조 컴포넌트, 다운 이벤트, 임시 자세·HUD 설정, 테스트 입력과 생존 인원 집계 연결을 선언한다.
// 작업자: 김세훈 | 2026-10-08 | 다운 캡슐 정렬 수정
// 변경 내용: 임시 다운 자세 설정이 메시 단독 회전 대신 캡슐과 몸을 함께 눕히도록 설명을 갱신한다.
// 작업자: 김세훈 | 2026-10-08 | 디버그 도움말 분리
// 변경 내용: 상태 HUD와 독립된 도움말 클래스·토글 키 설정과 로컬 입력·수명 관리를 추가한다.
// 작업자: 김세훈 | 2026-10-08 | 도움말 키 안내
// 변경 내용: HUD에서 실제 도움말 키와 사용 가능·열림 상태를 조회할 수 있게 한다.

// 역할: 입력과 로컬 카메라를 관리한다. 실제 이동/몸 회전/예측은 SPCharacterMovementComponent가 담당한다.
// NOTICE [TEMP-GRAVITY-SWITCH]: 정식 장치 도입 후 FindGravitySwitchInView/ServerUseGravitySwitch와
// Interact의 버튼 우선 분기를 교체한다. 기존 화물 상호작용과 영역 시스템은 유지한다.
// 작업자: 김세훈 (중력 영역/버튼 연동)

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SPGravityTypes.h"
#include "SPPlayerStatusComponent.h"
#include "SPPlayerCharacter.generated.h"

class ASPCargo;
class ASPGravitySwitch;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UAnimInstance;
class ULocalPlayer;
class USceneComponent;
class USPInventoryComponent;
class USPInteractorComponent;
class USPStealthActivityComponent;
class USPInteractableComponent;
class USPDebugHelpWidget;
class UUserWidget;
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
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;
    virtual void OnRep_Controller() override;

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    USPPlayerStatusComponent* GetStatusComponent() const { return Status; }

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    bool IsDowned() const;

    USPInteractableComponent* GetReviveInteraction() const { return ReviveInteraction; }

    /** 피해/다운 시험 입력은 개발 빌드에서만 사용한다. */
    bool AreStatusDebugControlsEnabled() const { return bEnableStatusDebugControls; }

    /** 개발 빌드의 로컬 도움말. HUD 표시 여부와 무관하며 다운 중에도 열고 닫을 수 있다. */
    UFUNCTION(BlueprintCallable, Category = "Player|Debug Help")
    void ToggleDebugHelp();

    UFUNCTION(BlueprintPure, Category = "Player|Debug Help")
    FKey GetDebugHelpKey() const { return DebugHelpKey; }

    UFUNCTION(BlueprintPure, Category = "Player|Debug Help")
    bool IsDebugHelpAvailable() const;

    UFUNCTION(BlueprintPure, Category = "Player|Debug Help")
    bool IsDebugHelpOpen() const { return DebugHelpWidget != nullptr; }

    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

    /** 애님 BP의 운반 포즈 전환용. 손에 물건이 있으면 true(등 가방은 제외). 인벤토리가 복제되므로 다른 플레이어 화면에서도 맞다. */
    UFUNCTION(BlueprintPure, Category = "Cargo")
    bool IsCarryingCargo() const;

    USPInventoryComponent* GetInventory() const;

    USPInteractorComponent* GetInteractor() const { return Interactor; }
    UFUNCTION(BlueprintPure, Category="Stealth") USPStealthActivityComponent* GetStealthActivity() const { return StealthActivity; }

    /** 진행 바 위젯용. E 길게 누르기의 진행률 0～1, 누르는 중이 아니면 0. */
    UFUNCTION(BlueprintPure, Category = "Interact")
    float GetHoldProgress() const;

protected:
    virtual void PostInitializeComponents() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
     * 손에 든 물건이 붙는 위치. 메시의 hand_r 소켓에 붙어 운반 포즈의 손을 따라간다.
     * 손 기준 위치/회전은 BP 컴포넌트 Details에서 맞춘다. 이름은 기존 BP 설정을 유지하려고 바꾸지 않는다.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<USceneComponent> CargoHoldPoint;

    /** 등 가방이 붙는 위치. 메시의 BackSocketName 소켓(또는 본)을 따라간다. 등 기준 위치/회전은 BP 컴포넌트 Details에서 맞춘다. */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<USceneComponent> BackPoint;

    /** 4칸 인벤토리. 서버 거리 검사, 버리기 여유 거리, 던지기 속도는 이 컴포넌트 Details에서 조절한다. */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<USPInventoryComponent> Inventory;

    /** E 길게 누르기. 서버 거리 확인과 진행 바 위젯은 이 컴포넌트 Details에서 정한다. */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<USPInteractorComponent> Interactor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
    TObjectPtr<USPStealthActivityComponent> StealthActivity;

    /** 체력, 피해, 다운과 구조의 규칙. 수치는 이 컴포넌트 Details에서 조정한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USPPlayerStatusComponent> Status;

    /** 쓰러진 플레이어를 대상으로 하는 기존 E 길게 누르기. 규칙은 Life가 연결한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USPInteractableComponent> ReviveInteraction;

    /** 임시 숫자 체력 UI. 별도 Widget Blueprint로 교체할 수 있다. */
    UPROPERTY(EditDefaultsOnly, Category = "Player|Status|Debug")
    TSubclassOf<UUserWidget> StatusHUDWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "Player|Status|Debug")
    bool bShowStatusDebugHUD = true;

    /** F6 피해 25, F7 다운, Shift+F7 호스트의 전체 초기화. Shipping/Test 빌드에서는 동작하지 않는다. */
    UPROPERTY(EditDefaultsOnly, Category = "Player|Status|Debug")
    bool bEnableStatusDebugControls = true;

    UPROPERTY(EditDefaultsOnly, Category = "Player|Debug Help")
    bool bEnableDebugHelp = true;

    /** 기본 H. F1~F5 등 언리얼 기본 디버그 키와 겹치지 않는 키를 사용한다. */
    UPROPERTY(EditDefaultsOnly, Category = "Player|Debug Help")
    FKey DebugHelpKey = EKeys::H;

    /** SPDebugHelpWidget을 상속한 BP에서 안내 목록과 외형을 수정할 수 있다. */
    UPROPERTY(EditDefaultsOnly, Category = "Player|Debug Help")
    TSubclassOf<USPDebugHelpWidget> DebugHelpWidgetClass;

    /** 캡슐과 몸을 함께 눕히는 임시 자세와 낮은 카메라. 정식 다운 처리로 교체할 때 끈다. */
    UPROPERTY(EditDefaultsOnly, Category = "Player|Status|Presentation")
    bool bUseTemporaryDownPose = true;

    UFUNCTION(BlueprintImplementableEvent, Category = "Player|Status", meta = (DisplayName = "On Status State Changed"))
    void BP_OnLifeStateChanged(ESPPlayerLifeState NewState);

    /** Axis2D: X=좌우, Y=전후. 키 배치는 기존 Input Mapping Context에서 설정한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction; // Space: 중력에서는 점프, 무중력에서는 몸 위쪽 추진.

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SprintAction; // Shift: 중력에서는 달리기, 무중력에서는 몸 아래쪽 추진.

    /** 선택 사항. 비어 있으면 Ctrl 유지형 액션/매핑을 런타임에 생성한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> CrouchAction;

    /** 기존 운반 애님 BP 뒤에 앉기 자세를 합성한다. 다른 스켈레톤은 호환 에셋으로 교체한다. */
    UPROPERTY(EditDefaultsOnly, Category = "Stealth|Animation")
    TSoftClassPtr<UAnimInstance> CrouchPoseClass = TSoftClassPtr<UAnimInstance>(
        FSoftObjectPath(TEXT("/Game/SpacePirate/Stealth/Animation/ABP_SPCrouchPostProcess.ABP_SPCrouchPostProcess_C")));

    // 인벤토리 입력. 비어 있으면 그 기능만 비활성화되고 다른 입력은 그대로 동작한다.
    /** E: 화면 중앙의 물건을 집는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> InteractAction;

    /** G: 짧게 누르면 현재 칸의 물건을 내려놓고, 길게 누르면 가방을 던진다. 액션에 Trigger를 넣지 않아야 누른 시간을 잴 수 있다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> DropAction;

    /** Axis1D: 숫자키 1～4에 Scalar 모디파이어로 1～4를 넣는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SelectSlotAction;

    /** Axis1D: 마우스 휠. 양수면 다음 칸, 음수면 이전 칸. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> CycleSlotAction;

    /** G를 이 시간(초)보다 오래 누르면 던지기다. MVP안에 없는 시험값이다. */
    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.0"))
    float ThrowHoldTime = 0.3f;

    /** 던지기로 판정된 뒤 최대 속도까지 차징하는 시간(초). MVP안에 없는 시험값이다. */
    UPROPERTY(EditDefaultsOnly, Category = "Input", meta = (ClampMin = "0.01"))
    float ThrowChargeTime = 1.0f;

    /** CargoHoldPoint가 붙을 메시 소켓(또는 본) 이름. 바꾸면 BP 뷰포트 미리보기에도 반영된다. */
    UPROPERTY(EditDefaultsOnly, Category = "Cargo")
    FName CargoHoldSocketName = TEXT("hand_r");

    /** BackPoint가 붙을 메시 소켓(또는 본) 이름. */
    UPROPERTY(EditDefaultsOnly, Category = "Cargo")
    FName BackSocketName = TEXT("spine_05");

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
    UFUNCTION()
    void HandleLifeStateChanged(ESPPlayerLifeState NewState);
    void EnsureStatusHUD();
    void RemoveStatusHUD();
    void RemoveDebugHelp();
    void DebugHelpInput(FKey Key, FInputActionValue Value);
    void DebugStatusKey(FKey Key, FInputActionValue Value);
    void DebugResetKey(FKey Key, FInputActionValue Value);
    void QueueTeamStatusRefresh();

    UFUNCTION(Server, Reliable)
    void ServerDebugLifeAction(uint8 Action);

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> StatusHUD;

    UPROPERTY(Transient)
    TObjectPtr<USPDebugHelpWidget> DebugHelpWidget;

    bool bAppliedDownState = false;
    bool bDropHeld = false;
    FVector StandingCameraLocation = FVector::ZeroVector;
    ECollisionResponse StandingVisibilityResponse = ECR_Ignore;

    void Move(const FInputActionValue& Value);
    void StopMove(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void StartJump();
    void EndJump();

    void StartSprint();
    void StopSprint();
    void HoldCrouch();
    void ReleaseCrouch();

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> CrouchMappingContext;
    TWeakObjectPtr<ULocalPlayer> CrouchLocalPlayer;

    void UpdateSprintRequest();

    // 클라이언트는 화면 기준으로 대상만 고르고, 판정과 부착은 인벤토리 컴포넌트가 서버에서 한다.
    void Interact();
    void StartDrop();
    void FinishDrop();
    void SelectSlot(const FInputActionValue& Value);
    void CycleSlot(const FInputActionValue& Value);
    void AttachPointToSocket(USceneComponent* Point, FName SocketName);

    // 키를 누른 상태를 유지해 중력 전환 후에도 같은 입력을 새 모드에서 해석한다.
    FVector2D MoveInput = FVector2D::ZeroVector;
    bool bSprintHeld = false;
    double DropPressedTime = 0.0;
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
