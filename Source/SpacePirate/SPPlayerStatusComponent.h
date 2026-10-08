#pragma once

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 신규 작성
// 변경 내용: 향후 스태미너 확장을 고려한 플레이어 상태 컴포넌트와 체력·다운·구조 API를 정의한다.

// 역할: 플레이어의 체력·피해·다운·구조 규칙을 한 곳에서 관리한다.
// 상태 변경은 서버에서만 수행하고, 체력과 생존 상태를 함께 복제해 화면에도 같은 결과를 알린다.
// 구조의 E 입력과 시간 측정은 기존 Interactable을 사용하고, 실제 이동·가방·작전 처리는 상태 알림에 연결한다.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPPlayerStatusComponent.generated.h"

class APawn;
class AController;
class UDamageType;
class USPInteractableComponent;

UENUM(BlueprintType)
enum class ESPPlayerLifeState : uint8
{
    Active,
    Downed
};

USTRUCT()
struct FSPPlayerStatusSnapshot
{
    GENERATED_BODY()

    UPROPERTY()
    float Health = 100.0f;

    UPROPERTY()
    ESPPlayerLifeState State = ESPPlayerLifeState::Active;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSPHealthChangedSignature, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPPlayerLifeStateChangedSignature, ESPPlayerLifeState, NewState);

UCLASS(ClassGroup = (SpacePirate), meta = (BlueprintSpawnableComponent))
class SPACEPIRATE_API USPPlayerStatusComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPPlayerStatusComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    float GetHealth() const { return StatusSnapshot.Health; }

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    float GetMaxHealth() const { return MaxHealth; }

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    float GetHealthPercent() const;

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    ESPPlayerLifeState GetLifeState() const { return StatusSnapshot.State; }

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    bool IsDowned() const { return StatusSnapshot.State == ESPPlayerLifeState::Downed; }

    /** 구조 대상과 구조자 모두 실제 플레이어여야 한다. 경비·자기 자신·다운된 구조자는 거부한다. */
    UFUNCTION(BlueprintPure, Category = "Player|Status")
    bool CanBeRevivedBy(APawn* Rescuer) const;

    /** 서버 전용. 0 이하·비정상 수치·다운 상태의 추가 피해는 무시한다. 일반 Apply Damage도 이 경로에 들어온다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Status")
    void ApplyDamage(float Amount);

    /** 서버 전용. 구조 상호작용이 완료됐을 때 조건을 다시 확인하고 체력을 회복한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Status")
    bool TryRevive(APawn* Rescuer);

    /** 서버 전용. 새 스테이지 시작 시 구조를 취소하고 최대 체력·활동 상태로 초기화한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Status")
    void ResetForStage();

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    float GetReviveProgress() const;

    UFUNCTION(BlueprintPure, Category = "Player|Status")
    APawn* GetRescuer() const;

    /** 서버의 변경 및 클라이언트의 복제 수신 시 호출된다. */
    UPROPERTY(BlueprintAssignable, Category = "Player|Status")
    FSPHealthChangedSignature OnHealthChanged;

    UPROPERTY(BlueprintAssignable, Category = "Player|Status")
    FSPPlayerLifeStateChangedSignature OnLifeStateChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Status|Health", meta = (ClampMin = "1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Status|Revive", meta = (ClampMin = "0.1", Units = "s"))
    float ReviveDuration = 4.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Status|Revive", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float ReviveHealthRatio = 0.3f;

    /** 작은 네트워크 위치 보정은 허용하되 시작 위치에서 이 거리 이상 움직이면 구조를 취소한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Player|Status|Revive", meta = (ClampMin = "0.0", Units = "cm"))
    float ReviveMovementTolerance = 5.0f;

private:
    UFUNCTION()
    void OnRep_StatusSnapshot(const FSPPlayerStatusSnapshot& PreviousSnapshot);

    UFUNCTION()
    void HandleAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
        AController* InstigatedBy, AActor* DamageCauser);

    void HandleReviveCompleted(APawn* Rescuer);
    void EnterDowned();
    void BroadcastChanges(const FSPPlayerStatusSnapshot& PreviousSnapshot, bool bForce = false);
    void CancelRevive();

    UPROPERTY(ReplicatedUsing = OnRep_StatusSnapshot)
    FSPPlayerStatusSnapshot StatusSnapshot;

    UPROPERTY(Transient)
    TObjectPtr<USPInteractableComponent> ReviveInteraction;
};
