#include "SPPlayerCharacter.h"

#include "SPCharacterMovementComponent.h"
#include "SPDebug.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"

ASPPlayerCharacter::ASPPlayerCharacter(
    const FObjectInitializer& ObjectInitializer)
    : Super(
        ObjectInitializer.SetDefaultSubobjectClass<
        USPCharacterMovementComponent>(
            ACharacter::CharacterMovementComponentName))
{
    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(34.0f, 96.0f);

    // 몸은 좌우 회전만 시선을 따라간다.
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;

    FirstPersonCamera =
        CreateDefaultSubobject<UCameraComponent>(
            TEXT("FirstPersonCamera"));

    FirstPersonCamera->SetupAttachment(GetCapsuleComponent());

    FirstPersonCamera->SetRelativeLocation(
        FVector(0.0f, 0.0f, 64.0f));

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
}

void ASPPlayerCharacter::Move(const FInputActionValue& Value)
{
    MoveInput = Value.Get<FVector2D>();

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
    if (GetCharacterMovement()->IsMovingOnGround())
    {
        Jump();
    }
}

void ASPPlayerCharacter::EndJump()
{
    StopJumping();
}

void ASPPlayerCharacter::StartSprint()
{
    bSprintHeld = true;

    UpdateSprintRequest();
}

void ASPPlayerCharacter::StopSprint()
{
    bSprintHeld = false;

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

    const bool bHasForwardInput = MoveInput.Y > 0.1f;
    const bool bRequested = bSprintHeld && bHasForwardInput;

    Movement->SetSprintRequested(bRequested);
}
