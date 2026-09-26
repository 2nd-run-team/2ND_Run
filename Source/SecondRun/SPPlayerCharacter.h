#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SPPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class SECONDRUN_API ASPPlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ASPPlayerCharacter(
        const FObjectInitializer& ObjectInitializer =
        FObjectInitializer::Get());

protected:
    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent) override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Components")
    TObjectPtr<UCameraComponent> FirstPersonCamera;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "Input",
        meta = (ClampMin = "0.01"))
    float MouseSensitivity = 1.0f;

private:
    void Move(const FInputActionValue& Value);
    void StopMove(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    void StartJump();
    void EndJump();

    void StartSprint();
    void StopSprint();

    void UpdateSprintRequest();

    FVector2D MoveInput = FVector2D::ZeroVector;
    bool bSprintHeld = false;
};