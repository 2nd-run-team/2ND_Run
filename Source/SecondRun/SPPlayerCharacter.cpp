#include "SPPlayerCharacter.h"

#include "SPCharacterMovementComponent.h"
#include "SPDebug.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Framework/Commands/InputChord.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

// NOTICE [TEMP-GRAVITY-TOGGLE]: 임시 '=' 키 전환 기능. 정식 중력 구역 도입 후 제거한다.
// 삭제 위치: SetupPlayerInputComponent의 BindDebugKey, 파일 끝의 DebugToggleGravity/RPC 구현, 헤더 선언.
// Shipping/Test에서는 바인딩과 실행 내용을 제외하며, RPC 선언과 빈 구현은 남는다.

ASPPlayerCharacter::ASPPlayerCharacter(
    const FObjectInitializer& ObjectInitializer)
    // 기본 이동 컴포넌트를 교체하여 Character의 기존 이동/예측 경로에서 커스텀 코드를 실행한다.
    : Super(
        ObjectInitializer.SetDefaultSubobjectClass<
        USPCharacterMovementComponent>(
            ACharacter::CharacterMovementComponentName))
{
    bReplicates = true;
    SetReplicateMovement(true);

    // 캡슐 크기, 카메라 위치/FOV는 생성 기본값이며 BP의 각 컴포넌트 Details에서 튜닝 가능하다.
    GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

    // 최초 중력 상태의 몸은 Yaw만 시선을 따른다. 무중력 전환 시 직접 추종을 끈다.
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    FirstPersonCamera =
        CreateDefaultSubobject<UCameraComponent>(
            TEXT("FirstPersonCamera"));

    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

    FirstPersonCamera->SetRelativeLocation(
        FVector(0.0f, 0.0f, 64.0f));

    // 시선은 즉시 반응하고, 몸은 이동 컴포넌트에서 별도로 천천히 회전한다.
    FirstPersonCamera->bUsePawnControlRotation = true;
    FirstPersonCamera->FieldOfView = 90.0f;

    GetMesh()->SetOwnerNoSee(true);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    JumpMaxCount = 1;
    JumpMaxHoldTime = 0.0f;
}

void ASPPlayerCharacter::SetupPlayerInputComponent(
    UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* Input =
        Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (!Input)
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Input setup aborted: expected EnhancedInputComponent, received %s. Check Project Settings > Input > Default Input Component Class."),
            *GetName(), *GetNameSafe(PlayerInputComponent));
        return;
    }

    if (!MoveAction || !LookAction || !JumpAction || !SprintAction)
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Input setup aborted: missing Input Action. Move=%s, Look=%s, Jump=%s, Sprint=%s. Assign all four actions in the player Blueprint Class Defaults."),
            *GetName(), *GetNameSafe(MoveAction.Get()), *GetNameSafe(LookAction.Get()),
            *GetNameSafe(JumpAction.Get()), *GetNameSafe(SprintAction.Get()));
        return;
    }

    // 유지형 이동은 Triggered로 갱신하고 Completed/Canceled 둘 다 처리해 입력이 남지 않게 한다.
    Input->BindAction(
        MoveAction,
        ETriggerEvent::Triggered,
        this,
        &ASPPlayerCharacter::Move);

    Input->BindAction(
        MoveAction,
        ETriggerEvent::Completed,
        this,
        &ASPPlayerCharacter::StopMove);

    Input->BindAction(
        MoveAction,
        ETriggerEvent::Canceled,
        this,
        &ASPPlayerCharacter::StopMove);

    Input->BindAction(
        LookAction,
        ETriggerEvent::Triggered,
        this,
        &ASPPlayerCharacter::Look);

    Input->BindAction(
        JumpAction,
        ETriggerEvent::Started,
        this,
        &ASPPlayerCharacter::StartJump);

    Input->BindAction(
        JumpAction,
        ETriggerEvent::Completed,
        this,
        &ASPPlayerCharacter::EndJump);

    Input->BindAction(
        JumpAction,
        ETriggerEvent::Canceled,
        this,
        &ASPPlayerCharacter::EndJump);

    Input->BindAction(
        SprintAction,
        ETriggerEvent::Started,
        this,
        &ASPPlayerCharacter::StartSprint);

    Input->BindAction(
        SprintAction,
        ETriggerEvent::Completed,
        this,
        &ASPPlayerCharacter::StopSprint);

    Input->BindAction(
        SprintAction,
        ETriggerEvent::Canceled,
        this,
        &ASPPlayerCharacter::StopSprint);

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // [TEMP-GRAVITY-TOGGLE] 테스트 전용이라 별도 Input Action 에셋 없이 임시로 바인딩한다.
    Input->BindDebugKey(
        FInputChord(EKeys::Equals),
        IE_Pressed,
        this,
        &ASPPlayerCharacter::DebugToggleGravity,
        false);
#endif
}

void ASPPlayerCharacter::Move(const FInputActionValue& Value)
{
    MoveInput = Value.Get<FVector2D>();

    RefreshZeroGravityInput();
    UpdateSprintRequest();

    if (!Controller)
    {
        if (!bReportedMissingController)
        {
            SP_DEBUG_LOG(Warning, TEXT("%s: Move input ignored: no Controller. Check possession and GameMode pawn/controller settings if this persists."),
                *GetName());
            bReportedMissingController = true;
        }
        return;
    }
    bReportedMissingController = false;

    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (Movement && Movement->IsZeroGravity())
    {
        // 무중력 이동은 이동 컴포넌트가 저장된 몸 기준 입력으로 계산.
        return;
    }

    // 중력 이동은 시선의 상하 각도를 무시한다. 하늘을 보며 W를 눌러도 수평으로 걷는다.
    const FRotator YawRotation(
        0.0f,
        Controller->GetControlRotation().Yaw,
        0.0f);

    const FVector Forward =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

    const FVector Right =
        FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    const FVector2D ClampedInput =
        MoveInput.GetClampedToMaxSize(1.0f);

    AddMovementInput(Forward, ClampedInput.Y);
    AddMovementInput(Right, ClampedInput.X);
}

void ASPPlayerCharacter::StopMove(const FInputActionValue& Value)
{
    MoveInput = FVector2D::ZeroVector;

    RefreshZeroGravityInput();
    UpdateSprintRequest();
}

void ASPPlayerCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D LookInput = Value.Get<FVector2D>();

    AddControllerYawInput(LookInput.X * MouseSensitivity);
    AddControllerPitchInput(LookInput.Y * MouseSensitivity);
}

void ASPPlayerCharacter::StartJump()
{
    // 같은 Space 상태를 모드에 따라 점프 또는 지속 추진으로 해석한다.
    bUpThrustHeld = true;
    RefreshZeroGravityInput();

    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (Movement && Movement->IsZeroGravity())
    {
        return;
    }

    if (GetCharacterMovement()->IsMovingOnGround())
    {
        Jump();
    }
}

void ASPPlayerCharacter::EndJump()
{
    bUpThrustHeld = false;
    RefreshZeroGravityInput();

    StopJumping();
}

void ASPPlayerCharacter::StartSprint()
{
    // Shift 상태는 무중력 아래 추진에도 사용하므로 달리기 가능 여부와 별도로 저장한다.
    bSprintHeld = true;

    RefreshZeroGravityInput();
    UpdateSprintRequest();
}

void ASPPlayerCharacter::StopSprint()
{
    bSprintHeld = false;

    RefreshZeroGravityInput();
    UpdateSprintRequest();
}

void ASPPlayerCharacter::UpdateSprintRequest()
{
    USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(
            GetCharacterMovement());

    if (!Movement)
    {
        if (!bReportedInvalidMovementComponent)
        {
            SP_DEBUG_LOG(Error, TEXT("%s: Sprint request ignored: movement component is not SPCharacterMovementComponent. Check the Blueprint parent and the character constructor's default subobject class."),
                *GetName());
            bReportedInvalidMovementComponent = true;
        }
        return;
    }
    bReportedInvalidMovementComponent = false;

    const bool bHasForwardInput = MoveInput.Y > FMath::Clamp(SprintForwardInputThreshold, 0.0f, 0.99f);
    const bool bRequested =
        !Movement->IsZeroGravity()
        && bSprintHeld
        && bHasForwardInput;

    Movement->SetSprintRequested(bRequested);
}

void ASPPlayerCharacter::RefreshZeroGravityInput()
{
    USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (!Movement)
    {
        return;
    }

    // Space와 Shift를 동시에 누르면 상하 추진이 상쇄된다. 입력은 이동 기록에 저장/전송된다.
    const float VerticalInput =
        (bUpThrustHeld ? 1.0f : 0.0f)
        - (bSprintHeld ? 1.0f : 0.0f);

    // 입력의 Y는 전후, X는 좌우.
    // 몸 로컬 좌표에서는 X=전후, Y=좌우, Z=상하.
    Movement->SetZeroGravityInput(
        FVector(
            MoveInput.Y,
            MoveInput.X,
            VerticalInput));
}

void ASPPlayerCharacter::FaceRotation(
    FRotator NewControlRotation,
    float DeltaTime)
{
    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (Movement && Movement->IsZeroGravity())
    {
        // 엔진의 즉시 시선 추종을 막는다. 몸 회전은 예측/재생되는 PhysCustom에서만 처리한다.
        return;
    }

    Super::FaceRotation(NewControlRotation, DeltaTime);
}

void ASPPlayerCharacter::OnMovementModeChanged(
    EMovementMode PrevMovementMode,
    uint8 PreviousCustomMode)
{
    Super::OnMovementModeChanged(
        PrevMovementMode, PreviousCustomMode);

    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    const bool bZeroGravity =
        Movement && Movement->IsZeroGravity();

    // 서버 변경뿐 아니라 복제로 모드가 바뀔 때도 동일한 회전/카메라 정책을 적용한다.
    bUseControllerRotationYaw = !bZeroGravity;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    if (bZeroGravity)
    {
        StopJumping();
    }

    UpdateCameraForMovementMode();
}

void ASPPlayerCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    // 도중 접속이나 다시 Possess된 경우에도 카메라 설정을 적용한다.
    UpdateCameraForMovementMode();
}

void ASPPlayerCharacter::UpdateCameraForMovementMode()
{
    APlayerController* PC =
        Cast<APlayerController>(Controller);

    if (!PC || !PC->IsLocalController()
        || !PC->PlayerCameraManager)
    {
        // 원격 플레이어나 아직 Possess되지 않은 상태는 정상.
        return;
    }

    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (!Movement)
    {
        return;
    }

    APlayerCameraManager* CameraManager =
        PC->PlayerCameraManager;

    if (Movement->IsZeroGravity())
    {
        if (!bCachedCameraPitchLimits)
        {
            SavedCameraPitchMin = CameraManager->ViewPitchMin;
            SavedCameraPitchMax = CameraManager->ViewPitchMax;
            bCachedCameraPitchLimits = true;
        }

        // 몸 회전 계산과 같은 BP 설정을 사용한다. 카메라만 다른 범위를 보지 않도록 한다.
        CameraManager->ViewPitchMin = FMath::Clamp(Movement->ZeroGravityPitchMin, -89.0f, 0.0f);
        CameraManager->ViewPitchMax = FMath::Clamp(Movement->ZeroGravityPitchMax, 0.0f, 89.0f);
    }
    else if (bCachedCameraPitchLimits)
    {
        CameraManager->ViewPitchMin = SavedCameraPitchMin;
        CameraManager->ViewPitchMax = SavedCameraPitchMax;
        bCachedCameraPitchLimits = false;
    }
}

// [TEMP-GRAVITY-TOGGLE] 클라이언트는 요청만 보낸다. 실제 전환은 서버 응답 후 적용된다.
void ASPPlayerCharacter::DebugToggleGravity(FKey Key, FInputActionValue ActionValue)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (!Movement)
    {
        SP_DEBUG_LOG(
            Error,
            TEXT("%s: Debug gravity change failed: missing SP movement component."),
            *GetName());

        return;
    }

    const ESPGravityMode TargetMode = Movement->IsZeroGravity()
        ? ESPGravityMode::Gravity
        : ESPGravityMode::ZeroGravity;

    ServerSetDebugGravityMode(TargetMode);
#endif
}

// [TEMP-GRAVITY-TOGGLE] 패키징 설정과 무관하게 Shipping/Test에서는 서버 측 변경도 실행하지 않는다.
void ASPPlayerCharacter::ServerSetDebugGravityMode_Implementation(
    ESPGravityMode NewMode)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (!Movement)
    {
        SP_DEBUG_LOG(
            Error,
            TEXT("%s: Server gravity change failed: missing SP movement component."),
            *GetName());

        return;
    }

    Movement->SetGravityMode(NewMode);
#endif
}
