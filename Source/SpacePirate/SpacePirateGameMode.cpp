// Copyright Epic Games, Inc. All Rights Reserved.

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: SPGameState 연결, 참가·이탈 및 다운 인원 집계, 전원 다운 판정과 전체 상태 초기화를 추가한다.

#include "SpacePirateGameMode.h"
#include "SPGuardAlertSubsystem.h"
#include "GameFramework/Controller.h"

#include "SPGameState.h"
#include "SPPlayerCharacter.h"
#include "SPPlayerStatusComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

ASpacePirateGameMode::ASpacePirateGameMode()
{
    GameStateClass = ASPGameState::StaticClass();
}

void ASpacePirateGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    // 미설정 기본 클래스만 보완한다. 열차/환경 GameState BP는 기존 기능을 보존하고 ASPGameState를 상속해야 한다.
    if (!GameStateClass || GameStateClass == AGameStateBase::StaticClass())
    {
        GameStateClass = ASPGameState::StaticClass();
    }
    ensureMsgf(GameStateClass->IsChildOf(ASPGameState::StaticClass()),
        TEXT("%s must inherit SPGameState to support team status; its custom class was preserved."),
        *GetNameSafe(GameStateClass));

    Super::InitGame(MapName, Options, ErrorMessage);
}

void ASpacePirateGameMode::RestartPlayer(AController* NewPlayer)
{
    Super::RestartPlayer(NewPlayer);
    CheckAllPlayersDowned();
}

void ASpacePirateGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    // Logout 도중에는 컨트롤러/폰이 아직 월드에 남아 있을 수 있다.
    UpdateTeamStatus(Exiting);
}

void ASpacePirateGameMode::CheckAllPlayersDowned()
{
    UpdateTeamStatus();
}

void ASpacePirateGameMode::UpdateTeamStatus(const AController* IgnoredController)
{
    if (!HasAuthority() || bResettingStage || !GetWorld())
    {
        return;
    }

    ASPGameState* OperationState = GetGameState<ASPGameState>();
    if (!OperationState)
    {
        return;
    }

    int32 ParticipatingCount = 0;
    int32 ActiveCount = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PlayerController = It->Get();
        if (!IsValid(PlayerController) || PlayerController == IgnoredController)
        {
            continue;
        }

        const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(PlayerController->GetPawn());
        if (IsValid(Player))
        {
            ++ParticipatingCount;
            ActiveCount += Player->IsDowned() ? 0 : 1;
        }
    }

    OperationState->SetTeamStatus(ActiveCount, ParticipatingCount);
    if (ParticipatingCount > 0 && ActiveCount == 0)
    {
        OperationState->MarkOperationFailed();
    }
}

void ASpacePirateGameMode::ResetForStage()
{
    if (!HasAuthority() || !GetWorld() || bResettingStage)
    {
        return;
    }

    // 체력 초기화 이벤트마다 전원 다운을 재검사하지 않도록 전체 초기화를 묶는다.
    {
        TGuardValue<bool> ResetGuard(bResettingStage, true);
        auto* Security = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
        // 보안 사건 접수를 끈 상태에서 이전 작업 취소 → 체력 복구 → 새 보안 스테이지 활성화 순으로 처리한다.
        // 취소 콜백이 새 스테이지 사건을 만들거나 초기화에 재진입하지 못하도록 이 순서를 유지한다.
        if (Security && !Security->EndStage()) { return; }
        if (ASPGameState* OperationState = GetGameState<ASPGameState>())
        {
            OperationState->ResetOperation();
        }

        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PlayerController = It->Get();
            ASPPlayerCharacter* Player = IsValid(PlayerController)
                ? Cast<ASPPlayerCharacter>(PlayerController->GetPawn()) : nullptr;
            if (IsValid(Player) && Player->GetStatusComponent())
            {
                Player->GetStatusComponent()->ResetForStage();
            }
        }
        if (Security) { Security->StartStage(); }
    }
    CheckAllPlayersDowned();
}

void ASpacePirateGameMode::InitGameState()
{
	Super::InitGameState();
	// BP에서 선택한 열차/하늘 GameState를 유지한 채 보안 상태만 연결한다.
	if (auto* Security = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>()) { Security->StartStage(); }
}

void ASpacePirateGameMode::GenericPlayerInitialization(AController* C)
{
	Super::GenericPlayerInitialization(C);
	if (C)
	{
		if (auto* Security = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>()) { Security->RegisterPlayer(C->PlayerState); }
	}
}
