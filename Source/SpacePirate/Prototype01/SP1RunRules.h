// 작성자 : 임진혁
#pragma once

#include "Prototype01/SP1Types.h"

/** 서버 라운드가 사용하는 작은 원장. 중복 화물과 중복 정산을 같은 경계에서 거부한다. */
struct FSP1RunLedger
{
    FGuid RunId;
    TSet<FName> CreditedCargo;
    int32 TeamValue = 0;
    int32 FinalValue = 0;
    bool bFinalized = false;

    bool Credit(const FGuid& RequestRun, FName CargoId, int32 Value)
    {
        if (RequestRun != RunId || bFinalized || CargoId.IsNone() || Value < 0
            || CreditedCargo.Contains(CargoId) || Value > MAX_int32 - TeamValue) return false;
        CreditedCargo.Add(CargoId);
        TeamValue += Value;
        return true;
    }

    bool Finalize(const FGuid& RequestRun, ESP1Phase Result)
    {
        if (RequestRun != RunId || bFinalized || !SP1::IsTerminal(Result)) return false;
        bFinalized = true;
        FinalValue = Result == ESP1Phase::Succeeded ? TeamValue : 0;
        return true;
    }
};

namespace SP1
{
    inline double MissionRemaining(const FSP1RunState& Run, double Now)
    { return Run.Maintenance.Phase == ESP1MaintenancePhase::Active ? Run.Maintenance.FrozenMissionRemaining : FMath::Max(0.,Run.MissionDeadline-Now); }

    // 정확한 정비 종료 시각으로 임무 마감을 옮긴다. 느린 프레임만큼 무료 시간을 더 주지 않는다.
    inline bool CloseMaintenance(FSP1RunState& Run, double ClosedAt, FName Reason)
    {
        if (Run.Maintenance.Phase != ESP1MaintenancePhase::Active) return false;
        Run.MissionDeadline = ClosedAt + Run.Maintenance.FrozenMissionRemaining;
        Run.Maintenance.Phase = ESP1MaintenancePhase::Closed;
        Run.Maintenance.CloseReason = Reason;
        return true;
    }
    inline bool CanRevive(const FSP1Participant& P)
    { return P.bConnected && !P.bAlive && !P.bMaintenanceRevived; }
    inline bool CanStartDeparture(const FSP1RunState& Run, bool bIntermediate, double Now)
    {
        return Run.Phase == ESP1Phase::Running && Run.DepartureId.IsNone() && MissionRemaining(Run,Now)>0
            && (!bIntermediate || (Run.Maintenance.Phase == ESP1MaintenancePhase::Active && Now < Run.Maintenance.Deadline));
    }
    struct FEndDecision { ESP1Phase Phase; FName Reason; };
    // 임무 마감은 완료/출발보다 우선한다. 유효 화물 반영은 이 검사 뒤, 출발 검사 전에 한다.
    inline bool MissionExpired(double Now, double Deadline) { return Now >= Deadline; }
    inline ESP1Phase DepartureResult(int32 Living, int32 Aboard)
    { return Living > 0 && Aboard > 0 ? ESP1Phase::Succeeded : ESP1Phase::Failed; }
    inline TOptional<FEndDecision> ResolveEnd(ESP1Phase Phase, double Now, double MissionDeadline,
        double DepartureDeadline, int32 Living, int32 Aboard)
    {
        if (!IsPlaying(Phase)) return {};
        if (MissionExpired(Now, MissionDeadline)) return FEndDecision{ESP1Phase::Failed, TEXT("MissionExpired")};
        if (Living <= 0) return FEndDecision{ESP1Phase::Failed, TEXT("NoConnectedSurvivors")};
        if (Phase == ESP1Phase::ExtractionCountdown && Now >= DepartureDeadline)
            return FEndDecision{DepartureResult(Living, Aboard), Aboard > 0 ? FName(TEXT("Departed")) : FName(TEXT("EmptyExtraction"))};
        return {};
    }
    inline bool MatchesAttempt(const FSP1Attempt& Attempt, const FGuid& Run, int32 Request, int32 Id)
    {
        return Attempt.bActive && Attempt.RunId == Run && Attempt.RequestId == Request
            && (Id == 0 || Attempt.AttemptId == Id);
    }
}
