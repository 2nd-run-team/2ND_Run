// Copyright Epic Games, Inc. All Rights Reserved.

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: SPGameState 연결, 참가·이탈 및 다운 인원 집계, 전원 다운 판정과 전체 상태 초기화를 추가한다.

#include "SpacePirateGameMode.h"

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
    // 기존 BP에 GameStateBase가 저장되어 있어도 실패 상태 복제가 동작하게 한다.
    // ASPGameState를 상속한 사용자 BP는 그대로 사용한다.
    if (!GameStateClass || !GameStateClass->IsChildOf(ASPGameState::StaticClass()))
    {
        GameStateClass = ASPGameState::StaticClass();
    }

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
    }
    CheckAllPlayersDowned();
}
