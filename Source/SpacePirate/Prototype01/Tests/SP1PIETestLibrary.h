// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SP1PIETestLibrary.generated.h"

class USP1InteractionComponent;
class USP1RoundComponent;
class ASPCargo;

/** 에디터 Python의 RPC 로컬 실행 보호 구간을 벗어나 실제 PIE 요청 경로를 검사한다. */
UCLASS()
class SPACEPIRATE_API USP1PIETestLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // PIE 외부에서는 아무 동작도 하지 않는다. 허용된 로컬 입력 메서드만 다음 게임 Tick에 호출한다.
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static void NextTick(USP1InteractionComponent* Interaction, FName Command);
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static void ConfigureTimers(USP1RoundComponent* Round, float MissionSeconds, float DepartureSeconds);
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static void SetRemainingSeconds(USP1RoundComponent* Round, float Seconds);
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static void WoundNextTick(AActor* Target, float Amount);
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static ASPCargo* SpawnCarryFixture(AActor* Context, FVector Position);
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static void PickupNextTick(APawn* Pawn, ASPCargo* Cargo);
    // 판정 경계 회귀 시험용. 제품 라운드 완료/점수 함수를 우회하지 않고 마감만 맞춘다.
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static void AlignMaintenanceDeadlineToAttempt(USP1InteractionComponent* Interaction);
    UFUNCTION(BlueprintCallable, meta=(DevelopmentOnly), Category="Prototype01|Test")
    static void AlignMissionDeadlineToDeparture(USP1RoundComponent* Round);
};
