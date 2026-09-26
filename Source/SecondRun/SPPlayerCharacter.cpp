#include "SPPlayerCharacter.h"

#include "SPCharacterMovementComponent.h"

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
        CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

    checkf(
        MoveAction && LookAction && JumpAction && SprintAction,
        TEXT("Assign all Input Actions on BP_SPPlayerCharacter."));

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
        return;
    }

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
        CastChecked<USPCharacterMovementComponent>(
            GetCharacterMovement());

    const bool bHasForwardInput = MoveInput.Y > 0.1f;

    Movement->SetSprintRequested(
        bSprintHeld && bHasForwardInput);
}