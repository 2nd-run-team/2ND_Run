#pragma once
#include "CoreMinimal.h"
#include "SPStealthTypes.generated.h"

UENUM(BlueprintType)
enum class ESPCrimeKind : uint8 { None, Pickpocket, SecurityTerminal, VaultWork, LootPacking, SuppressionTool };
class APlayerState;

UENUM(BlueprintType)
enum class ESPStealthIncident : uint8
{
    SustainedCrimeConfirmed, InstantCrimeWitnessed, DirectReportCompleted,
    LaserContact, WorkNoise, IndirectReport, VictimReport, IdentifiedPlayerRediscovered, EscapeActivated
};
UENUM(BlueprintType)
enum class ESPStealthResult : uint8
{
    Applied, Duplicate, NotAuthority, InactiveStage, StaleStage, InvalidRequest, UnknownIdentity
};
/** 같은 사건의 재시도는 IncidentId를 유지한다. StageId는 이전 스테이지의 지연된 완료/신고를 거절하는 기준이다. */
USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPStealthIncidentContext
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid IncidentId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid StageId;
    /** 사건 당시 위치를 고정해 전달한다. 숨은 플레이어를 따라 계속 갱신하는 좌표가 아니다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
    /** None이면 출동하지 않는다. 전체 경비를 뜻하지 않으며 신원 공유 범위와도 별개다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DispatchGroup;
    /** 사건 위치에서 경비까지의 3차원 거리(cm). 0이면 출동하지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float ResponseRadius = 5000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) int32 MaxResponders = 4;
    /** 목격한 행동을 설명하는 부가 정보. 신원·경보 처리 정책은 사건 종류(ESPStealthIncident)가 결정한다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ESPCrimeKind CrimeKind = ESPCrimeKind::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRestrictedArea = false;
};
/** 사건 처리 당시의 결과 기록. 현재 신원/경보는 PlayerState/GameState 컴포넌트에서 조회한다. */
USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPStealthIncidentRecord
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FSPStealthIncidentContext Context;
    UPROPERTY(BlueprintReadOnly) ESPStealthIncident Kind = ESPStealthIncident::LaserContact;
    UPROPERTY(BlueprintReadOnly) double ServerTime = 0;
    UPROPERTY(BlueprintReadOnly) ESPStealthResult Result = ESPStealthResult::InvalidRequest;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<APlayerState> Player;
    UPROPERTY(BlueprintReadOnly) FName IdentityScope;
    UPROPERTY(BlueprintReadOnly) bool bFirstIdentification = false;
    UPROPERTY(BlueprintReadOnly) bool bNewIdentityScope = false;
    UPROPERTY(BlueprintReadOnly) bool bFirstGlobalAlarm = false;
    UPROPERTY(BlueprintReadOnly) int32 DispatchedGuards = 0;
};
USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPStealthIdentityState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid StageId;
    /** 하나라도 있으면 발각된 상태다. 다른 공유 범위가 추가되어도 최초 발각 이벤트는 반복하지 않는다. */
    UPROPERTY(BlueprintReadOnly) TArray<FName> KnownToScopes;
    UPROPERTY(BlueprintReadOnly) double FirstIdentifiedAt = -1;
};
USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPStealthAlarmState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid StageId;
    UPROPERTY(BlueprintReadOnly) bool bStageActive = false;
    UPROPERTY(BlueprintReadOnly) bool bGlobalAlarm = false;
    UPROPERTY(BlueprintReadOnly) double AlarmServerTime = -1;
    UPROPERTY(BlueprintReadOnly) FGuid AlarmIncidentId;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSPStealthStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPStealthIncidentEvent, const FSPStealthIncidentRecord&, Record);
DECLARE_MULTICAST_DELEGATE_OneParam(FSPStealthIncidentNative, const FSPStealthIncidentRecord&);
