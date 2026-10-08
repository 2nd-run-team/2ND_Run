#pragma once

#include "CoreMinimal.h"
#include "SPPlayerCharacter.h"
#include "SPStealthTypes.h"
#include "SPStealthObserverComponent.h"
#include "SPGuardCharacter.generated.h"

class ASPGuardPatrolRoute;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ESPGuardState : uint8
{
    // Preserve existing serialized enum values. Searching means last-seen scene search.
    Patrol, Suspicious, Pursuing, Searching, Listening, Investigating,
    SceneSearching, MovingToLastSeen
};

UENUM(BlueprintType)
enum class ESPGuardMoveFailure : uint8 { None, NoController, NoNavigation, Unreachable, Blocked, Stalled, TimedOut };
UENUM(BlueprintType)
enum class ESPGuardInvestigationResult : uint8 { None, Completed, Superseded, InterruptedBySighting, Failed, StageReset };

/** Completed response receipt, not another incident/identity state owner. */
USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPGuardInvestigationReceipt
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid IncidentId;
    UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) ESPGuardInvestigationResult Result = ESPGuardInvestigationResult::None;
    UPROPERTY(BlueprintReadOnly) ESPGuardMoveFailure Failure = ESPGuardMoveFailure::None;
    UPROPERTY(BlueprintReadOnly) int32 MoveRequests = 0;
    UPROPERTY(BlueprintReadOnly) double ServerTime = 0;
};

/** 팀 플레이어와 동일한 캐릭터/이동 기반. AI 판정과 이동 명령은 서버에서만 실행한다. */
UCLASS()
class SPACEPIRATE_API ASPGuardCharacter : public ASPPlayerCharacter
{
    GENERATED_BODY()
public:
    ASPGuardCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMode, uint8 PreviousCustomMode = 0) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Guard|Patrol")
    TObjectPtr<ASPGuardPatrolRoute> PatrolRoute;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Patrol", meta = (ClampMin = "0"))
    int32 StartPointIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Patrol", meta = (ClampMin = "0"))
    float PatrolWaitTime = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Movement", meta = (ClampMin = "1"))
    float PatrolSpeed = 170.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Movement", meta = (ClampMin = "1"))
    float ChaseSpeed = 340.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Sight", meta = (ClampMin = "1"))
    float SightDistance = 1200.0f;

    /** 전체 수평 시야각 (반각이 아님). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Sight", meta = (ClampMin = "1", ClampMax = "179"))
    float SightAngle = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Sight", meta = (ClampMin = "1", ClampMax = "179"))
    float VerticalSightAngle = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Sight", meta = (ClampMin = "0"))
    float ConfirmSightTime = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Hearing")
    bool bHearFootsteps = false;

    /** 앉지 않고 지상에서 이동하는 플레이어의 발소리 반경(cm). 벽 너머 소리도 방향만 확인한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Hearing", meta = (ClampMin = "0"))
    float HearingDistance = 650.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Hearing", meta = (ClampMin = "1"))
    float MinimumFootstepSpeed = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Hearing", meta = (ClampMin = "0.1"))
    float ListenTime = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Hearing", meta = (ClampMin = "1"))
    float HearingTurnSpeed = 240.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Search", meta = (ClampMin = "0.1"))
    float SearchTime = 5.0f;

    /** 마지막 위치에 접근할 수 없어도 무한 추격하지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Search", meta = (ClampMin = "1"))
    float LostTargetTimeout = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Movement", meta=(ClampMin="0", ClampMax="10"))
    int32 MaxMoveRetries = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Movement", meta=(ClampMin="0.1"))
    float MoveRetryInterval = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Movement", meta=(ClampMin="0.5"))
    float StuckTimeout = 3.0f;
    /** Travel budget only; the full SearchTime begins after arrival. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Movement", meta=(ClampMin="1"))
    float MoveTimeout = 30.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Movement", meta=(ClampMin="0.1"))
    float FailedResponseCooldown = 5.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Alert", meta=(ClampMin="0"))
    float SightingResponseRadius = 5000;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Alert", meta=(ClampMin="0"))
    int32 SightingMaxResponders = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Alert")
    FName AlertGroup = TEXT("FreightGuards");

    /** Identity knowledge is independent of AlertGroup (the actual dispatch group). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Alert")
    FName IdentityScope = TEXT("StageSecurity");

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Guard|State")
    bool bInvestigatingAnonymousIncident = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Guard|State")
    FVector InvestigationLocation = FVector::ZeroVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Guard|State")
    FGuid InvestigationIncidentId;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Guard|State")
    FSPGuardInvestigationReceipt LastInvestigation;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Guard|State")
    ESPGuardMoveFailure LastMoveFailure = ESPGuardMoveFailure::None;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Guard|State")
    int32 MoveRequestCount = 0;

    /** Text component/font/material remain replaceable, independently of AI rules. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Guard|Feedback")
    bool bShowStateIndicator = true;

    /** 개발 빌드에서 각 클라이언트 화면에 엄폐로 잘리는 시야 부채꼴을 그린다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Debug")
    bool bShowVision = true;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Guard|State")
    ESPGuardState GuardState = ESPGuardState::Patrol;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Guard|State")
    TObjectPtr<ASPPlayerCharacter> TargetPlayer;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Guard|State")
    FVector LastSeenLocation = FVector::ZeroVector;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Guard|State")
    float SuspicionProgress = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category = "Guard|State")
    FVector HeardLocation = FVector::ZeroVector;

    UFUNCTION(BlueprintPure, Category = "Guard|Sight")
    bool CanSeePlayer(const ASPPlayerCharacter* Player) const;

    UFUNCTION(BlueprintPure, Category = "Guard|Hearing")
    bool CanHearPlayer(const ASPPlayerCharacter* Player) const;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Guard|Sight")
    TObjectPtr<USPStealthObserverComponent> Observer;
    void ConfigureObserver();

    // 경보 시스템 내부 서버 진입점. RPC가 아니며 클라이언트 호출은 무시한다.
    bool ReceiveSighting(ASPPlayerCharacter* Player, const FVector& Location);
    bool ReceiveAnonymousIncident(const FSPStealthIncidentContext& Incident);
    void ResetSecurityResponse();

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Guard|Feedback")
    TObjectPtr<UTextRenderComponent> AlertIndicator;

private:
    void UpdateSight(float DeltaSeconds);
    void UpdateHearing(float DeltaSeconds);
    void UpdateBehavior(float DeltaSeconds);
    void UpdateFeedback();
    void DrawVision() const;
    enum class EMoveProgress : uint8 { Moving, Arrived, Failed };
    EMoveProgress MoveToLocation(const FVector& Location, float AcceptanceRadius, float DeltaSeconds);
    EMoveProgress FailMove(ESPGuardMoveFailure Failure);
    void ResetMovementTask();
    void PauseMovement();
    bool HasReached(const FVector& Location, float Radius) const;
    void FinishInvestigation(ESPGuardInvestigationResult Result);
    void AdvancePatrolPoint();
    FVector GetSightOrigin() const;
    void ReturnToPatrol();

    void HandleWitness(const FSPStealthWitness& Witness);
    int32 PatrolIndex = 0;
    int32 PatrolDirection = 1;
    float PatrolWaitRemaining = 0.0f;
    float SearchRemaining = 0.0f;
    float LastSightingTime = 0.0f;
    float NextSignalTime = 0.0f;
    float MoveRetryRemaining = 0;
    float MoveElapsed = 0;
    float NoProgressElapsed = 0;
    float ResponseCooldownRemaining = 0;
    float PatrolFailureWait = 0;
    int32 MoveFailures = 0;
    bool bMoveActive = false;
    bool bMoveExhausted = false;
    FVector ProgressLocation = FVector::ZeroVector;
    FVector RequestedDestination = FVector(UE_BIG_NUMBER);
    bool bTargetVisible = false;
    TWeakObjectPtr<ASPPlayerCharacter> HeardPlayer;
    float ListeningRemaining = 0;
    float FootstepSampleRemaining = 0;
    bool bAtInvestigationLocation = false;
};
