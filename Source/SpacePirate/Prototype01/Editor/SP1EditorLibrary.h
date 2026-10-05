// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SP1EditorLibrary.generated.h"

/** 지원되는 UMG 에디터 API로 전용 BP에 편집 가능한 위젯 배치를 한 번 생성한다. */
UCLASS()
class SPACEPIRATE_API USP1EditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Prototype01|Editor")
    static bool BuildHUDTemplate(UObject* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Prototype01|Editor")
    static bool AddSurvivalHUD(UObject* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Prototype01|Editor")
    static bool AddMechanicHUD(UObject* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Prototype01|Editor")
    static bool BuildEvaluationHUD(UObject* Blueprint,bool bRebuild = false);
};
