// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SP1ScenarioDefinition.generated.h"
class USP1CargoDefinition;

/** 인원별 시험 시간을 별도 자산으로 저장한다. 원안 비교는 명시적인 Override로 선택한다. */
UCLASS(BlueprintType)
class SPACEPIRATE_API USP1RunSettings : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName SettingsId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) float MissionSeconds = 420;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 IntendedPlayers = 4;
};

USTRUCT(BlueprintType)
struct FSP1CargoBudgetRow
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName CarId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TObjectPtr<USP1CargoDefinition> Definition;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 Count = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bGravityCargo = false;
};

/** 배치 표와 실제 월드 화물의 종류/객차/수량을 시작 전에 대조한다. */
UCLASS(BlueprintType)
class SPACEPIRATE_API USP1ScenarioDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> CarOrder;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FSP1CargoBudgetRow> CargoBudget;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ExpectedNormal = 40;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ExpectedSpecial = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 ExpectedValue = 7060;
};
