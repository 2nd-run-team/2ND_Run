// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SP1CargoDefinition.generated.h"

class USoundAttenuation;

class UStaticMesh;
class USoundBase;
class UTexture2D;

/** 일반 자동 전송 화물의 서버 시험값과 표현. 기존 Small/Mid/Large 운반과 독립적이다. */
UCLASS(BlueprintType)
class SPACEPIRATE_API USP1CargoDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText BadgeLabel;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FLinearColor BadgeColor = FLinearColor(0.2f,0.8f,1.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 Value = 100;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.2")) float HoldSeconds = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector MeshScale = FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector MeshOffset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector InteractionOffset = FVector(0,0,45);
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundBase> CompleteSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USoundAttenuation> CompleteAttenuation;
};
