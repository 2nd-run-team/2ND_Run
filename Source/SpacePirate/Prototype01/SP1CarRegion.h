// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SP1CarRegion.generated.h"
class UBoxComponent;

/** 객차 내부만 명시한다. 연결 통로는 포함하지 않아 위험이 이웃 칸으로 번지지 않는다. */
UCLASS()
class SPACEPIRATE_API ASP1CarRegion : public AActor
{
    GENERATED_BODY()
public:
    ASP1CarRegion();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FName CarId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FText HazardLabel;
    UFUNCTION(BlueprintPure) bool ContainsPoint(FVector Point) const;
    static ASP1CarRegion* FindAt(const UObject* Context, FVector Point);
};
