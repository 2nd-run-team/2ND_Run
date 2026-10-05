// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SP1ExtractionZone.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class APawn;

UCLASS()
class SPACEPIRATE_API ASP1ExtractionZone : public AActor
{
    GENERATED_BODY()
public:
    ASP1ExtractionZone();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Zone;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Terminal;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName DepartureId = TEXT("P01_C10_Final");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DepartureLabel;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIntermediate = false;
    // 최종 탈출의 전방(-Local X) 200cm까지 모든 피해 요청을 서버에서 제외한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bProtectFromHazards = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="200")) float SafeApproachCentimeters = 200;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector InteractionOffset = FVector(180,0,20);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.2")) float HoldSeconds = 1;
    UFUNCTION(BlueprintPure) bool ContainsPawn(const APawn* Pawn) const;
    UFUNCTION(BlueprintPure) FVector GetInteractionPoint() const;
    bool ProtectsPawn(const APawn* Pawn) const;
};
