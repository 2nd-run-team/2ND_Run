#pragma once

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 신규 작성
// 변경 내용: 작전 실패 알림과 인원·실패 상태 조회 및 서버 갱신 API를 정의한다.

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SPGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSPOperationFailed);

/** 모든 클라이언트가 공유하는 작전 결과. 실패 판정은 서버 GameMode가 한다. */
UCLASS()
class SPACEPIRATE_API ASPGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Operation")
    bool IsOperationFailed() const { return bOperationFailed; }

    UFUNCTION(BlueprintPure, Category = "Operation")
    int32 GetActivePlayerCount() const { return ActivePlayerCount; }

    UFUNCTION(BlueprintPure, Category = "Operation")
    int32 GetParticipatingPlayerCount() const { return ParticipatingPlayerCount; }

    /** 서버와 클라이언트에서 실패로 전환될 때 한 번 발생한다. 늦게 참가한 UI는 IsOperationFailed도 확인한다. */
    UPROPERTY(BlueprintAssignable, Category = "Operation")
    FSPOperationFailed OnOperationFailed;

    void SetTeamStatus(int32 ActiveCount, int32 ParticipatingCount);
    void MarkOperationFailed();
    void ResetOperation();

private:
    UPROPERTY(ReplicatedUsing = OnRep_OperationFailed)
    bool bOperationFailed = false;

    UPROPERTY(Replicated)
    int32 ActivePlayerCount = 0;

    UPROPERTY(Replicated)
    int32 ParticipatingPlayerCount = 0;

    UFUNCTION()
    void OnRep_OperationFailed();
};
