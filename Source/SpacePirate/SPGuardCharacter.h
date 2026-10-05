#pragma once

#include "CoreMinimal.h"
#include "SPPlayerCharacter.h"
#include "SPGuardCharacter.generated.h"

class ASPGuardPatrolRoute;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ESPGuardState : uint8
{
    Patrol, Suspicious, Pursuing, Searching, Listening
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
    bool bHearFootsteps = true;

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Guard|Alert")
    FName AlertGroup = TEXT("FreightGuards");

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

    // 경보 시스템 내부 서버 진입점. RPC가 아니며 클라이언트 호출은 무시한다.
    void ReceiveSighting(ASPPlayerCharacter* Player, const FVector& Location);

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
    void MoveToLocation(const FVector& Location, float AcceptanceRadius);
    FVector GetSightOrigin() const;
    void ReturnToPatrol();

    TMap<TWeakObjectPtr<ASPPlayerCharacter>, float> ConfirmationTimes;
    int32 PatrolIndex = 0;
    int32 PatrolDirection = 1;
    float PatrolWaitRemaining = 0.0f;
    float SearchRemaining = 0.0f;
    float LastSightingTime = 0.0f;
    float NextSignalTime = 0.0f;
    float NextMoveTime = 0.0f;
    FVector RequestedDestination = FVector(UE_BIG_NUMBER);
    bool bTargetVisible = false;
    TWeakObjectPtr<ASPPlayerCharacter> HeardPlayer;
    float ListeningRemaining = 0;
    float FootstepSampleRemaining = 0;
};
