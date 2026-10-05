// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "SP1Types.generated.h"

class APlayerState;

UENUM(BlueprintType)
enum class ESP1Phase : uint8 { Ready, Running, ExtractionCountdown, Succeeded, Failed, Aborted };

UENUM(BlueprintType)
enum class ESP1MaintenancePhase : uint8 { Unused, Active, Closed };

UENUM(BlueprintType)
enum class ESP1PingKind : uint8 { Location, Cargo, Danger };

/** 핑은 승인 시점의 고정 좌표다. Actor 참조나 추적 대상은 저장하지 않는다. */
USTRUCT(BlueprintType)
struct FSP1Ping
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Id = 0;
    UPROPERTY(BlueprintReadOnly) int32 PlayerId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) ESP1PingKind Kind = ESP1PingKind::Location;
    UPROPERTY(BlueprintReadOnly) double ExpiresAt = 0;
};

USTRUCT(BlueprintType)
struct FSP1MaintenanceState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ESP1MaintenancePhase Phase = ESP1MaintenancePhase::Unused;
    UPROPERTY(BlueprintReadOnly) double StartedAt = 0;
    UPROPERTY(BlueprintReadOnly) double Deadline = 0;
    UPROPERTY(BlueprintReadOnly) double FrozenMissionRemaining = 0;
    UPROPERTY(BlueprintReadOnly) bool bForwardReached = false;
    UPROPERTY(BlueprintReadOnly) FName CloseReason;
};

/** 한 번에 복제하는 라운드 스냅샷. UI는 로컬 완료를 점수로 계산하지 않는다. */
USTRUCT(BlueprintType)
struct FSP1RunState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid RunId;
    UPROPERTY(BlueprintReadOnly) ESP1Phase Phase = ESP1Phase::Ready;
    UPROPERTY(BlueprintReadOnly) double MissionDeadline = 0;
    UPROPERTY(BlueprintReadOnly) double ExtractionDeadline = 0;
    UPROPERTY(BlueprintReadOnly) FSP1MaintenanceState Maintenance;
    UPROPERTY(BlueprintReadOnly) FName DepartureId;
    UPROPERTY(BlueprintReadOnly) FText DepartureLabel;
    UPROPERTY(BlueprintReadOnly) FName SettingsId;
    UPROPERTY(BlueprintReadOnly) float MissionLimit = 0;
    UPROPERTY(BlueprintReadOnly) int32 TeamValue = 0;
    UPROPERTY(BlueprintReadOnly) int32 FinalValue = 0;
    UPROPERTY(BlueprintReadOnly) int32 Aboard = 0;
    UPROPERTY(BlueprintReadOnly) int32 Living = 0;
    UPROPERTY(BlueprintReadOnly) int32 Escaped = 0;
    UPROPERTY(BlueprintReadOnly) int32 LeftBehind = 0;
    UPROPERTY(BlueprintReadOnly) FString Reason;
};

USTRUCT(BlueprintType)
struct FSP1Participant
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> PlayerState;
    UPROPERTY(BlueprintReadOnly) int32 PlayerId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) bool bReady = false;
    UPROPERTY(BlueprintReadOnly) bool bConnected = true;
    UPROPERTY(BlueprintReadOnly) bool bAlive = true;
    UPROPERTY(BlueprintReadOnly) bool bEscaped = false;
    UPROPERTY(BlueprintReadOnly) bool bMaintenanceRevived = false;
    UPROPERTY(BlueprintReadOnly) FName CarId;
    UPROPERTY(BlueprintReadOnly) float Wounds = 0;
};

/** 소유자에게만 복제한다. RequestId는 입력 상관관계, AttemptId는 서버가 발급한 시도 번호다. */
USTRUCT(BlueprintType)
struct FSP1Attempt
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid RunId;
    UPROPERTY(BlueprintReadOnly) int32 RequestId = 0;
    UPROPERTY(BlueprintReadOnly) int32 AttemptId = 0;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target;
    UPROPERTY(BlueprintReadOnly) double StartedAt = 0;
    UPROPERTY(BlueprintReadOnly) float Duration = 0;
    UPROPERTY(BlueprintReadOnly) bool bActive = false;
    UPROPERTY(BlueprintReadOnly) FName Reason;
};

namespace SP1
{
    inline bool IsPlaying(ESP1Phase Phase)
    { return Phase == ESP1Phase::Running || Phase == ESP1Phase::ExtractionCountdown; }
    inline bool IsTerminal(ESP1Phase Phase)
    { return Phase == ESP1Phase::Succeeded || Phase == ESP1Phase::Failed || Phase == ESP1Phase::Aborted; }
}
