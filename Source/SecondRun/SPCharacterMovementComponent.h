#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SPCharacterMovementComponent.generated.h"

UCLASS()
class SECONDRUN_API USPCharacterMovementComponent
    : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    USPCharacterMovementComponent();

    void SetSprintRequested(bool bRequested);

    bool IsSprintRequested() const
    {
        return bSprintRequested;
    }

    virtual float GetMaxSpeed() const override;

    virtual FNetworkPredictionData_Client*
        GetPredictionData_Client() const override;

    virtual bool ClientUpdatePositionAfterServerUpdate() override;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Movement|Sprint",
        meta = (ClampMin = "0.0"))
    float SprintSpeed = 700.0f;

protected:
    virtual void BeginPlay() override;

    virtual void UpdateFromCompressedFlags(uint8 Flags) override;

private:
    bool HasForwardAcceleration() const;

    bool bSprintRequested = false;
};
