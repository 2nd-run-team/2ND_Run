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

    /** 이 시험 구역 안의 미발각 플레이어만 침입자로 확인한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stealth")
    TObjectPtr<UBoxComponent> RestrictedArea;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patrol")
    bool bLoop = true;

    UFUNCTION(BlueprintPure, Category = "Patrol")
    FVector GetPatrolLocation(int32 Index) const;

    UFUNCTION(BlueprintPure, Category = "Stealth")
    bool ContainsLocation(FVector Location) const;
};
