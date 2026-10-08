// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 신규 작성
// 변경 내용: 활동·참가 인원과 작전 실패 상태를 서버에서 갱신하고 클라이언트에 복제한다.

#include "SPGameState.h"

#include "Net/UnrealNetwork.h"

void ASPGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPGameState, bOperationFailed);
    DOREPLIFETIME(ASPGameState, ActivePlayerCount);
    DOREPLIFETIME(ASPGameState, ParticipatingPlayerCount);
}

void ASPGameState::SetTeamStatus(int32 ActiveCount, int32 ParticipatingCount)
{
    if (!HasAuthority())
    {
        return;
    }

    ParticipatingPlayerCount = FMath::Max(0, ParticipatingCount);
    ActivePlayerCount = FMath::Clamp(ActiveCount, 0, ParticipatingPlayerCount);
    ForceNetUpdate();
}

void ASPGameState::MarkOperationFailed()
{
    if (!HasAuthority() || bOperationFailed)
    {
        return;
    }

    bOperationFailed = true;
    OnRep_OperationFailed();
    ForceNetUpdate();
}

void ASPGameState::ResetOperation()
{
    if (HasAuthority())
    {
        bOperationFailed = false;
        ForceNetUpdate();
    }
}

void ASPGameState::OnRep_OperationFailed()
{
    if (bOperationFailed)
    {
        OnOperationFailed.Broadcast();
    }
}
