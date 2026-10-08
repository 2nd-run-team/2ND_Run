// Copyright Epic Games, Inc. All Rights Reserved.

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 서버의 참가자 집계·작전 실패·상태 초기화 API와 모듈 외부 접근용 API 매크로를 추가한다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SpacePirateGameMode.generated.h"

/** 서버가 참가자의 생존 상태를 집계하고 전원 다운 시 작전 실패를 결정한다. */
UCLASS(abstract)
class SPACEPIRATE_API ASpacePirateGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASpacePirateGameMode();
	virtual void InitGameState() override;
	virtual void GenericPlayerInitialization(AController* C) override;

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    /** 플레이어 생존 상태 변경 후 서버에서 호출. 참가자가 있고 전원 다운이면 작전을 한 번 실패시킨다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Operation")
    void CheckAllPlayersDowned();

    /** 서버의 전체 회차 초기화: 작업 취소, 체력/생존/작전 실패 복구, 신원/경보/경비 대응 초기화.
     * 물건 재생성이나 플레이어 위치 이동은 수행하지 않는다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Operation")
    void ResetForStage();

private:
    void UpdateTeamStatus(const AController* IgnoredController = nullptr);
    bool bResettingStage = false;
};



