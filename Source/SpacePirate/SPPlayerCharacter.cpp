#include "SPPlayerCharacter.h"

#include "SPGravitySwitch.h"
#include "SPCharacterMovementComponent.h"
#include "SPDebug.h"
#include "SPInventoryComponent.h"
#include "SPCargo.h"

#include "Engine/World.h"

#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

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

    // 인벤토리 입력은 선택 사항이다. 비어 있는 액션의 기능만 꺼지고 이동 입력은 그대로 동작한다.
    if (InteractAction)
    {
        Input->BindAction(
            InteractAction,
            ETriggerEvent::Started,
            this,
            &ASPPlayerCharacter::Interact);
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
        !Movement->IsCustomGravityMovement()
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
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (ASPGravitySwitch* Switch = FindGravitySwitchInView())
    {
        ServerUseGravitySwitch(Switch);
        return;
    }
#endif

    // 클라이언트에서 미리 걸러 불필요한 RPC를 줄인다. 최종 판정은 서버가 같은 규칙으로 다시 한다.
    ASPCargo* Item = FindItemInView();
    if (Item && Inventory->CanPickUp(Item))
    {
        Inventory->ServerPickUp(Item);
    }
}

void ASPPlayerCharacter::StartDrop()
{
    DropPressedTime = GetWorld()->GetTimeSeconds();
}

void ASPPlayerCharacter::FinishDrop()
{
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
    // 숫자키마다 Scalar 모디파이어로 1～4를 넣어 한 액션으로 받는다.
    Inventory->ServerSelectSlot(
        FMath::RoundToInt(Value.Get<float>()) - 1);
}

void ASPPlayerCharacter::CycleSlot(const FInputActionValue& Value)
{
    const float Axis = Value.Get<float>();
    if (!FMath::IsNearlyZero(Axis))
    {
        Inventory->ServerCycleSlot(Axis > 0.0f ? 1 : -1);
    }
}

ASPCargo* ASPPlayerCharacter::FindItemInView() const
{
    if (!Controller)
    {
        return nullptr;
    }

    // 몸 기준 눈높이는 무중력에서 몸이 기울면 화면과 어긋나므로, 실제 카메라 시점에서 쏜다.
    FVector ViewLocation;
    FRotator ViewRotation;
    Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

    const FVector TraceEnd =
        ViewLocation + ViewRotation.Vector() * CargoTraceDistance;

    const FCollisionQueryParams Params(
        SCENE_QUERY_STAT(SPItemTrace), false, this);

    FHitResult Hit;
    const bool bHit = GetWorld()->SweepSingleByChannel(
        Hit,
        ViewLocation,
        TraceEnd,
        FQuat::Identity,
        CargoTraceChannel,
        FCollisionShape::MakeSphere(CargoTraceRadius),
        Params);

    return bHit ? Cast<ASPCargo>(Hit.GetActor()) : nullptr;
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
    if (!IsValid(TargetSwitch) || FindGravitySwitchInView() != TargetSwitch)
    {
        // 지연 중 대상/시선이 바뀌는 정상 상황은 로그를 남기지 않는다.
        return;
    }
    TargetSwitch->TryActivate();
#endif
}
