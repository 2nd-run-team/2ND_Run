// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 상태·구조 컴포넌트를 연결하고 다운 시 입력·이동 제한, 가방 낙하, 임시 자세·카메라 복구를 추가한다.
// 변경 내용: 소유자 HUD·테스트 키와 생존 인원 집계 알림을 연결한다.
// 작업자: 김세훈 | 2026-10-08 | 다운 캡슐 정렬 수정
// 변경 내용: 메시 단독 회전을 제거하고 서버의 캡슐 회전에 자세를 맞춘다. 다운 카메라는 캡슐 중심에 둔다.
// 작업자: 김세훈 | 2026-10-08 | 디버그 도움말 분리
// 변경 내용: H 토글과 대괄호 페이지 입력, 별도 도움말의 로컬 생성·제거를 연결한다.
// 작업자: 김세훈 | 2026-10-08 | 도움말 키 안내
// 변경 내용: 도움말이 꺼져 있거나 개발용 입력이 없는 빌드에서 HUD 키 안내를 숨기도록 조회 함수를 추가한다.
// 작업자: 김세훈 | 2026-10-08 | 도움말 페이지 설정 변경
// 변경 내용: 이전·다음 페이지 입력을 대괄호에서 쉼표·마침표로 변경한다.

#include "SPPlayerCharacter.h"

#include "SPGravitySwitch.h"
#include "SPCharacterMovementComponent.h"
#include "SPDebug.h"
#include "SPDebugHelpWidget.h"
#include "SPInteractorComponent.h"
#include "SPInteractableComponent.h"
#include "SPPlayerStatusHUDWidget.h"
#include "SPGravityWorldSubsystem.h"
#include "SpacePirateGameMode.h"
#include "SPInventoryComponent.h"
#include "SPCargo.h"
#include "SPStealthActivityComponent.h"

#include "Engine/World.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Animation/AnimInstance.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "Framework/Commands/InputChord.h"
#include "TimerManager.h"

// NOTICE [TEMP-GRAVITY-SWITCH]: 정식 장치 도입 후 Interact의 버튼 분기와 버튼 검색/RPC를 교체한다.
// 작업자: 김세훈 (중력 영역/버튼 연동). Shipping/Test에서는 임시 버튼 사용을 제외한다.

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

    // 운반 포즈의 오른손을 따라간다. 상대 위치/회전은 손 기준이며 BP에서 맞춘다.
    CargoHoldPoint =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("CargoHoldPoint"));

    // 생성자에서는 C++ 기본 소켓으로 붙이고, BP에서 바꾼 소켓은 OnConstruction에서 다시 붙인다.
    CargoHoldPoint->SetupAttachment(GetMesh(), CargoHoldSocketName);

    BackPoint =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("BackPoint"));

    BackPoint->SetupAttachment(GetMesh(), BackSocketName);

    Inventory =
        CreateDefaultSubobject<USPInventoryComponent>(
            TEXT("Inventory"));

    Inventory->SetAttachPoints(CargoHoldPoint, BackPoint);

    Interactor =
        CreateDefaultSubobject<USPInteractorComponent>(
            TEXT("Interactor"));

    Status = CreateDefaultSubobject<USPPlayerStatusComponent>(TEXT("Status"));
    ReviveInteraction = CreateDefaultSubobject<USPInteractableComponent>(TEXT("ReviveInteraction"));
    StatusHUDWidgetClass = USPPlayerStatusHUDWidget::StaticClass();
    DebugHelpWidgetClass = USPDebugHelpWidget::StaticClass();

    JumpMaxCount = 1;
    StealthActivity = CreateDefaultSubobject<USPStealthActivityComponent>(TEXT("StealthActivity"));
    JumpMaxHoldTime = 0.0f;
}

void ASPPlayerCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    Status->OnLifeStateChanged.AddUniqueDynamic(this, &ASPPlayerCharacter::HandleLifeStateChanged);
}

void ASPPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (GetMesh()->GetSkeletalMeshAsset() && !CrouchPoseClass.IsNull())
    {
        if (UClass* PoseClass = CrouchPoseClass.LoadSynchronous())
        {
            GetMesh()->SetOverridePostProcessAnimBP(PoseClass);
        }
    }
    HandleLifeStateChanged(Status->GetLifeState());
    EnsureStatusHUD();
}

void ASPPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority()) { StealthActivity->CancelAllActivity(); }
    RemoveStatusHUD();
    RemoveDebugHelp();
    if (CrouchLocalPlayer.IsValid() && CrouchMappingContext)
    {
        if (auto* Subsystem = CrouchLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            Subsystem->RemoveMappingContext(CrouchMappingContext);
        }
    }
    Super::EndPlay(EndPlayReason);
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

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bEnableDebugHelp && DebugHelpKey.IsValid())
    {
        Input->BindDebugKey(FInputChord(DebugHelpKey), IE_Pressed, this, &ASPPlayerCharacter::DebugHelpInput, false);
        if (DebugHelpKey != EKeys::Comma)
        {
            Input->BindDebugKey(FInputChord(EKeys::Comma), IE_Pressed, this, &ASPPlayerCharacter::DebugHelpInput, false);
        }
        if (DebugHelpKey != EKeys::Period)
        {
            Input->BindDebugKey(FInputChord(EKeys::Period), IE_Pressed, this, &ASPPlayerCharacter::DebugHelpInput, false);
        }
    }
    if (bEnableStatusDebugControls)
    {
        Input->BindDebugKey(FInputChord(EKeys::F6), IE_Pressed, this, &ASPPlayerCharacter::DebugStatusKey, false);
        Input->BindDebugKey(FInputChord(EKeys::F7), IE_Pressed, this, &ASPPlayerCharacter::DebugStatusKey, false);
        Input->BindDebugKey(FInputChord(EKeys::F7, true, false, false, false), IE_Pressed,
            this, &ASPPlayerCharacter::DebugResetKey, false);
    }
#endif

    // 기존 팀 IMC/BP를 재저장하지 않아도 Ctrl 유지형 앉기가 동작한다.
    // Crouch의 bWantsToCrouch는 엔진 SavedMove의 기본 압축 플래그로 예측/복제된다.
    if (!CrouchAction)
    {
        CrouchAction = NewObject<UInputAction>(this, TEXT("IA_SPHoldCrouch"));
        CrouchAction->ValueType = EInputActionValueType::Boolean;
        CrouchMappingContext = NewObject<UInputMappingContext>(this);
        CrouchMappingContext->MapKey(CrouchAction, EKeys::LeftControl);
        CrouchMappingContext->MapKey(CrouchAction, EKeys::RightControl);
    }
    if (CrouchMappingContext)
    {
        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
            {
                if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                {
                    CrouchLocalPlayer = LocalPlayer;
                    Subsystem->AddMappingContext(CrouchMappingContext, 1);
                }
            }
        }
    }
    Input->BindAction(CrouchAction, ETriggerEvent::Triggered, this, &ASPPlayerCharacter::HoldCrouch);
    Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ASPPlayerCharacter::ReleaseCrouch);
    Input->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ASPPlayerCharacter::ReleaseCrouch);

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

    // 인벤토리 입력은 선택 사항이다. 비어 있는 액션의 기능만 꺼지고 이동 입력은 그대로 동작한다.
    if (InteractAction)
    {
        Input->BindAction(
            InteractAction,
            ETriggerEvent::Started,
            this,
            &ASPPlayerCharacter::Interact);

        // 길게 누르기는 손을 떼면 취소한다.
        Input->BindAction(
            InteractAction,
            ETriggerEvent::Completed,
            Interactor.Get(),
            &USPInteractorComponent::StopInteract);
        Input->BindAction(InteractAction, ETriggerEvent::Canceled,
            Interactor.Get(), &USPInteractorComponent::StopInteract);
    }

    // 짧게/길게를 놓을 때 누른 시간으로 가른다.
    if (DropAction)
    {
        Input->BindAction(
            DropAction,
            ETriggerEvent::Started,
            this,
            &ASPPlayerCharacter::StartDrop);

        Input->BindAction(
            DropAction,
            ETriggerEvent::Completed,
            this,
            &ASPPlayerCharacter::FinishDrop);
    }

    if (SelectSlotAction)
    {
        Input->BindAction(
            SelectSlotAction,
            ETriggerEvent::Started,
            this,
            &ASPPlayerCharacter::SelectSlot);
    }

    // 휠은 한 프레임짜리 입력이라, 연속으로 굴려도 매번 들어오도록 Triggered로 받는다.
    if (CycleSlotAction)
    {
        Input->BindAction(
            CycleSlotAction,
            ETriggerEvent::Triggered,
            this,
            &ASPPlayerCharacter::CycleSlot);
    }

    if (!InteractAction || !DropAction || !SelectSlotAction || !CycleSlotAction)
    {
        SP_DEBUG_LOG(Warning, TEXT("%s: Inventory input partly disabled: Interact=%s, Drop=%s, SelectSlot=%s, CycleSlot=%s. Assign them in the player Blueprint Class Defaults."),
            *GetName(), *GetNameSafe(InteractAction.Get()), *GetNameSafe(DropAction.Get()),
            *GetNameSafe(SelectSlotAction.Get()), *GetNameSafe(CycleSlotAction.Get()));
    }

}

void ASPPlayerCharacter::Move(const FInputActionValue& Value)
{
    // 금고 직접 해제처럼 조작을 잠그는 대상을 누르는 동안에는 움직이지 않는다.
    if (IsDowned() || Interactor->IsControlLocked())
    {
        return;
    }

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

    if (Movement && Movement->IsCustomGravityMovement())
    {
        // 무중력/복귀 이동은 저장된 입력으로 이동 컴포넌트가 계산한다.
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
    if (Interactor->IsControlLocked())
    {
        return;
    }

    const FVector2D LookInput = Value.Get<FVector2D>();

    AddControllerYawInput(LookInput.X * MouseSensitivity);
    AddControllerPitchInput(LookInput.Y * MouseSensitivity);
}

void ASPPlayerCharacter::StartJump()
{
    if (IsDowned() || Interactor->IsHolding()) { return; }
    // 같은 Space 상태를 모드에 따라 점프 또는 지속 추진으로 해석한다.
    bUpThrustHeld = true;
    RefreshZeroGravityInput();

    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (Movement && Movement->IsCustomGravityMovement())
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

void ASPPlayerCharacter::HoldCrouch()
{
    if (IsDowned() || Interactor->IsHolding()) { return; }
    Crouch();
}

void ASPPlayerCharacter::ReleaseCrouch()
{
    if (IsDowned()) { return; }
    UnCrouch();
}

void ASPPlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    // 카메라는 캡슐 부착이므로 메시 보정과 별개로 눈높이도 낮춘다.
    FirstPersonCamera->AddLocalOffset(FVector(0, 0, -HalfHeightAdjust * 0.5f));
}

void ASPPlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    FirstPersonCamera->AddLocalOffset(FVector(0, 0, HalfHeightAdjust * 0.5f));
}

void ASPPlayerCharacter::StartSprint()
{
    if (IsDowned()) { return; }
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
        !IsDowned()
        && !Movement->IsCustomGravityMovement()
        && bSprintHeld
        && bHasForwardInput;

    Movement->SetSprintRequested(bRequested);
}

void ASPPlayerCharacter::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 생성자 시점에는 BP에서 바꾼 소켓 이름이 아직 적용되지 않는다.
    AttachPointToSocket(CargoHoldPoint, CargoHoldSocketName);
    AttachPointToSocket(BackPoint, BackSocketName);
}

void ASPPlayerCharacter::AttachPointToSocket(USceneComponent* Point, FName SocketName)
{
    if (Point->GetAttachSocketName() != SocketName)
    {
        Point->AttachToComponent(
            GetMesh(),
            FAttachmentTransformRules::KeepRelativeTransform,
            SocketName);
    }

    // 에디터 편집 중 반복 호출되므로 게임 월드에서만 알린다.
    // 소켓이 없으면 물건이 메시 원점(발밑)에 붙는다.
    if (GetWorld() && GetWorld()->IsGameWorld()
        && GetMesh()->GetSkeletalMeshAsset()
        && !GetMesh()->DoesSocketExist(SocketName))
    {
        SP_DEBUG_LOG(Warning, TEXT("%s: Socket '%s' for %s not found on mesh %s. Check the socket/bone name in the player Blueprint Class Defaults."),
            *GetName(), *SocketName.ToString(), *Point->GetName(),
            *GetNameSafe(GetMesh()->GetSkeletalMeshAsset()));
    }
}

bool ASPPlayerCharacter::IsCarryingCargo() const
{
    return Inventory->GetHandItem() != nullptr;
}

USPInventoryComponent* ASPPlayerCharacter::GetInventory() const
{
    return Inventory;
}

// 작업자: 김세훈 | 임시 버튼을 우선 사용하고 기존 물건 상호작용으로 이어간다.
void ASPPlayerCharacter::Interact()
{
    if (IsDowned()) { return; }
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (ASPGravitySwitch* Switch = FindGravitySwitchInView())
    {
        ServerUseGravitySwitch(Switch);
        return;
    }
#endif

    // 줍기를 포함한 E 상호작용은 Interactor가 화면 중앙 대상을 찾아 처리한다.
    Interactor->Press();
}

float ASPPlayerCharacter::GetHoldProgress() const
{
    return Interactor->GetHoldProgress();
}

void ASPPlayerCharacter::StartDrop()
{
    // 길게 누르는 동안에는 이동과 시점 회전 말고 다른 조작은 무시한다.
    if (IsDowned() || Interactor->IsHolding())
    {
        return;
    }

    bDropHeld = true;
    DropPressedTime = GetWorld()->GetTimeSeconds();
}

void ASPPlayerCharacter::FinishDrop()
{
    const bool bWasHeld = bDropHeld;
    bDropHeld = false;
    if (!bWasHeld || IsDowned() || Interactor->IsHolding())
    {
        return;
    }

    if (!Inventory->GetActiveItem())
    {
        return;
    }

    const double HeldTime = GetWorld()->GetTimeSeconds() - DropPressedTime;
    if (HeldTime < ThrowHoldTime)
    {
        Inventory->ServerDrop();
        return;
    }

    // 던지기로 판정된 뒤부터 차징한다. 가방이 아니면 서버가 내려놓기로 처리한다.
    const double Charge = (HeldTime - ThrowHoldTime) / FMath::Max(ThrowChargeTime, 0.01f);
    Inventory->ServerThrow(static_cast<float>(FMath::Clamp(Charge, 0.0, 1.0)));
}

void ASPPlayerCharacter::SelectSlot(const FInputActionValue& Value)
{
    if (IsDowned() || Interactor->IsHolding())
    {
        return;
    }

    // 숫자키마다 Scalar 모디파이어로 1～4를 넣어 한 액션으로 받는다.
    Inventory->ServerSelectSlot(
        FMath::RoundToInt(Value.Get<float>()) - 1);
}

void ASPPlayerCharacter::CycleSlot(const FInputActionValue& Value)
{
    if (IsDowned() || Interactor->IsHolding())
    {
        return;
    }

    const float Axis = Value.Get<float>();
    if (!FMath::IsNearlyZero(Axis))
    {
        Inventory->ServerCycleSlot(Axis > 0.0f ? 1 : -1);
    }
}

void ASPPlayerCharacter::RefreshZeroGravityInput()
{
    USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (!Movement)
    {
        return;
    }

    if (IsDowned())
    {
        Movement->SetZeroGravityInput(FVector::ZeroVector);
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
    if (IsDowned()) { return; }
    const USPCharacterMovementComponent* Movement =
        Cast<USPCharacterMovementComponent>(GetCharacterMovement());

    if (Movement && Movement->IsCustomGravityMovement())
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

    const bool bCustomGravity =
        Movement && Movement->IsCustomGravityMovement();

    // 서버 변경뿐 아니라 복제로 모드가 바뀔 때도 동일한 회전/카메라 정책을 적용한다.
    bUseControllerRotationYaw = !bCustomGravity;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    if (bCustomGravity)
    {
        StopJumping();
    }

    // 이 콜백은 보정 후 과거 이동 재생 중에도 실행된다.
    // 여기서 현재 키 상태를 입력에 다시 쓰면 SavedMove의 과거 입력을 오염시킨다.
    UpdateCameraForMovementMode();
}

void ASPPlayerCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    // 도중 접속이나 다시 Possess된 경우에도 카메라 설정을 적용한다.
    UpdateCameraForMovementMode();
    EnsureStatusHUD();
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

    if (Movement->IsCustomGravityMovement())
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

// 작업자: 김세훈 | [TEMP-GRAVITY-SWITCH] 화면 또는 서버 조준 방향에서 거리/가림을 검사한다.
ASPGravitySwitch* ASPPlayerCharacter::FindGravitySwitchInView() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!Controller || !FirstPersonCamera || !GetWorld())
    {
        return nullptr;
    }

    FVector ViewLocation;
    FRotator ViewRotation;
    if (IsLocallyControlled())
    {
        Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
    }
    else
    {
        // 원격 플레이어에는 서버의 로컬 화면 카메라를 사용하지 않는다.
        ViewLocation = FirstPersonCamera->GetComponentLocation();
        ViewRotation = GetBaseAimRotation();
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SPGravitySwitchTrace), false, this);
    if (ASPCargo* HandItem = Inventory->GetHandItem())
    {
        Params.AddIgnoredActor(HandItem);
    }
    const FVector End = ViewLocation
        + ViewRotation.Vector() * FMath::Max(GravitySwitchUseDistance, 1.0f);
    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, ECC_Visibility, Params))
    {
        return Cast<ASPGravitySwitch>(Hit.GetActor());
    }
#endif
    return nullptr;
}

// 작업자: 김세훈 | [TEMP-GRAVITY-SWITCH] 클라이언트의 대상 포인터를 서버 시선 검사로 검증한다.
void ASPPlayerCharacter::ServerUseGravitySwitch_Implementation(ASPGravitySwitch* TargetSwitch)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (IsDowned() || !IsValid(TargetSwitch) || FindGravitySwitchInView() != TargetSwitch)
    {
        // 지연 중 대상/시선이 바뀌는 정상 상황은 로그를 남기지 않는다.
        return;
    }
    TargetSwitch->TryActivate();
#endif
}

bool ASPPlayerCharacter::IsDowned() const
{
    return Status && Status->IsDowned();
}

void ASPPlayerCharacter::HandleLifeStateChanged(ESPPlayerLifeState NewState)
{
    Interactor->StopInteract();
    bDropHeld = false;
    const bool bDowned = NewState == ESPPlayerLifeState::Downed;
    // 다운 여부의 원본은 Status다. 잠입 기능은 이 결과를 받아 작업을 취소하며, 소생해도 신원 발각 기록은 유지한다.
    if (HasAuthority()) { StealthActivity->SetIncapacitated(bDowned); }
    USPCharacterMovementComponent* Movement = Cast<USPCharacterMovementComponent>(GetCharacterMovement());
    if (bDowned && !bAppliedDownState)
    {
        bAppliedDownState = true;
        MoveInput = FVector2D::ZeroVector;
        bSprintHeld = false;
        bUpThrustHeld = false;
        bDropHeld = false;
        DropPressedTime = 0.0;
        StopJumping();
        ConsumeMovementInputVector();
        Interactor->StopInteract();
        if (Movement)
        {
            Movement->SetSprintRequested(false);
            Movement->SetZeroGravityInput(FVector::ZeroVector);
            Movement->StopMovementImmediately();
            if (HasAuthority() && bUseTemporaryDownPose)
            {
                Movement->TryEnterDownedPose();
            }
            Movement->DisableMovement();
        }
        if (HasAuthority()) { Inventory->DropBag(); }

        // 회전된 캡슐이 몸 전체의 구조 판정도 담당한다. 클라이언트는 서버 이동/회전 복제를 따른다.
        StandingVisibilityResponse = GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Visibility);
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        StandingCameraLocation = FirstPersonCamera->GetRelativeLocation();
        if (bUseTemporaryDownPose)
        {
            // 메시의 기본 오프셋과 네트워크 보정은 이동 컴포넌트가 유지한다.
            // 보정 중의 상대 변환을 새 기준으로 저장하면 원격 몸이 다시 서는 문제가 생긴다.
            FirstPersonCamera->SetRelativeLocation(FVector(StandingCameraLocation.X, StandingCameraLocation.Y, 0.0));
        }
    }
    else if (!bDowned && bAppliedDownState)
    {
        bAppliedDownState = false;
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, StandingVisibilityResponse);
        if (bUseTemporaryDownPose)
        {
            FirstPersonCamera->SetRelativeLocation(StandingCameraLocation);
        }
        // 다운 중 Ctrl 해제 입력이 생략됐더라도 이전 웅크리기 요청이 남지 않는다.
        UnCrouch();
        if (Movement)
        {
            // 다운 중 환경이 바뀌었을 수 있으므로 복귀 시 현재 영역의 중력을 읽는다.
            const USPGravityWorldSubsystem* Gravity = GetWorld()->GetSubsystem<USPGravityWorldSubsystem>();
            const bool bZeroGravity = Gravity
                && Gravity->GetGravityModeAtLocation(GetActorLocation()) == ESPGravityMode::ZeroGravity;
            Movement->SetMovementMode(MOVE_Custom, bZeroGravity
                ? USPCharacterMovementComponent::ZeroGravityCustomMode
                : USPCharacterMovementComponent::GravityRecoveryCustomMode);
        }
    }
    BP_OnLifeStateChanged(NewState);
    if (HasAuthority() && IsPlayerControlled() && GetController()->GetPawn() == this)
    {
        if (ASpacePirateGameMode* Mode = Cast<ASpacePirateGameMode>(GetWorld()->GetAuthGameMode()))
        {
            Mode->CheckAllPlayersDowned();
        }
    }
}

void ASPPlayerCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    EnsureStatusHUD();
    QueueTeamStatusRefresh();
}

void ASPPlayerCharacter::UnPossessed()
{
    if (HasAuthority()) { StealthActivity->CancelAllActivity(); }
    Interactor->StopInteract();
    RemoveStatusHUD();
    RemoveDebugHelp();
    Super::UnPossessed();
    QueueTeamStatusRefresh();
}

void ASPPlayerCharacter::QueueTeamStatusRefresh()
{
    // PossessedBy 중에는 Controller->Pawn이 아직 갱신 전이다. 빙의가 끝난 뒤 인원을 집계한다.
    if (HasAuthority())
    {
        if (ASpacePirateGameMode* Mode = Cast<ASpacePirateGameMode>(GetWorld()->GetAuthGameMode()))
        {
            GetWorld()->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateWeakLambda(Mode, [Mode]() { Mode->CheckAllPlayersDowned(); }));
        }
    }
}

void ASPPlayerCharacter::OnRep_Controller()
{
    Super::OnRep_Controller();
    if (IsLocallyControlled()) { EnsureStatusHUD(); }
    else { RemoveStatusHUD(); RemoveDebugHelp(); }
}

void ASPPlayerCharacter::EnsureStatusHUD()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!StatusHUD && bShowStatusDebugHUD && StatusHUDWidgetClass
        && PC && PC->IsLocalController() && PC->GetLocalPlayer() && HasActorBegunPlay())
    {
        StatusHUD = CreateWidget<UUserWidget>(PC, StatusHUDWidgetClass);
        if (StatusHUD)
        {
            StatusHUD->AddToPlayerScreen(10);
            StatusHUD->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
}

void ASPPlayerCharacter::RemoveStatusHUD()
{
    if (StatusHUD)
    {
        StatusHUD->RemoveFromParent();
        StatusHUD = nullptr;
    }
}

void ASPPlayerCharacter::DebugStatusKey(FKey Key, FInputActionValue Value)
{
    if (Key == EKeys::F6) { ServerDebugLifeAction(0); }
    else if (Key == EKeys::F7) { ServerDebugLifeAction(1); }
}

bool ASPPlayerCharacter::IsDebugHelpAvailable() const
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return bEnableDebugHelp && DebugHelpWidgetClass && DebugHelpKey.IsValid();
#else
    return false;
#endif
}

void ASPPlayerCharacter::ToggleDebugHelp()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (DebugHelpWidget)
    {
        RemoveDebugHelp();
        return;
    }
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!bEnableDebugHelp || !DebugHelpWidgetClass || !HasActorBegunPlay()
        || !PC || !PC->IsLocalController() || !PC->GetLocalPlayer())
    {
        return;
    }
    DebugHelpWidget = CreateWidget<USPDebugHelpWidget>(PC, DebugHelpWidgetClass);
    if (DebugHelpWidget)
    {
        DebugHelpWidget->Configure(DebugHelpKey, bEnableStatusDebugControls);
        DebugHelpWidget->AddToPlayerScreen(20);
    }
#endif
}

void ASPPlayerCharacter::RemoveDebugHelp()
{
    if (DebugHelpWidget)
    {
        DebugHelpWidget->RemoveFromParent();
        DebugHelpWidget = nullptr;
    }
}

void ASPPlayerCharacter::DebugHelpInput(FKey Key, FInputActionValue Value)
{
    if (Key == DebugHelpKey) { ToggleDebugHelp(); }
    else if (DebugHelpWidget)
    {
        DebugHelpWidget->ChangePage(Key == EKeys::Period ? 1 : -1);
    }
}

void ASPPlayerCharacter::DebugResetKey(FKey Key, FInputActionValue Value) { ServerDebugLifeAction(2); }

void ASPPlayerCharacter::ServerDebugLifeAction_Implementation(uint8 Action)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!bEnableStatusDebugControls || !IsPlayerControlled()) { return; }
    if (Action <= 1)
    {
        UGameplayStatics::ApplyDamage(this, Action == 0 ? 25.0f : Status->GetMaxHealth(),
            GetController(), this, UDamageType::StaticClass());
    }
    else if (Action == 2 && GetController()->IsLocalController())
    {
        if (ASpacePirateGameMode* Mode = Cast<ASpacePirateGameMode>(GetWorld()->GetAuthGameMode()))
        {
            Mode->ResetForStage();
        }
    }
#endif
}
