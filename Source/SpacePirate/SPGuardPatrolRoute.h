#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPGuardPatrolRoute.generated.h"

class UBoxComponent;

/** 차량 기준으로 이동/회전할 수 있는 순찰 경로. Points는 바닥 높이의 로컬 좌표다. */
UCLASS()
class SPACEPIRATE_API ASPGuardPatrolRoute : public AActor
{
    GENERATED_BODY()
public:
    ASPGuardPatrolRoute();
    virtual void Tick(float DeltaSeconds) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol", meta = (MakeEditWidget = "true"))
    TArray<FVector> Points;

    /** Legacy authoring bounds only. Detection uses independent ASPRestrictedArea actors. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stealth")
    TObjectPtr<UBoxComponent> RestrictedArea;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol")
    bool bLoop = true;

    UFUNCTION(BlueprintPure, Category = "Patrol")
    FVector GetPatrolLocation(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Stealth", meta=(DeprecatedFunction,DeprecationMessage="Use SPRestrictedArea. Patrol bounds no longer determine trespass."))
    bool ContainsLocation(FVector Location) const;
};
