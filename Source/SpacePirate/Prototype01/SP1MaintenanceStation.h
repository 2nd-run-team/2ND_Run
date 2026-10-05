// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SP1MaintenanceStation.generated.h"
class UStaticMeshComponent;
class ASP1CarRegion;

/** 배치와 조작 지점만 갖는다. 정비 시계/부활/종료는 기존 GameState의 Round가 판정한다. */
UCLASS()
class SPACEPIRATE_API ASP1MaintenanceStation : public AActor
{
    GENERATED_BODY()
public:
    ASP1MaintenanceStation();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Terminal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<ASP1CarRegion> Region;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName StationId = TEXT("P01_C05_Revive");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector InteractionOffset = FVector(0,0,30);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.2")) float HoldSeconds = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1",ClampMax="30")) float WindowSeconds = 30;
    // 캡슐 중심의 월드 위치는 이 상대 위치에 Actor Transform을 적용해 얻는다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FVector> ReviveOffsets;
    UFUNCTION(BlueprintPure) FVector GetInteractionPoint() const;
};
