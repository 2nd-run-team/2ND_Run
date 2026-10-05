// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Prototype01/SP1SurvivalRules.h"
#include "SP1SurvivalComponent.generated.h"

class APawn;
class USP1RoundComponent;

/** 전용 Pawn의 생존 상태. 소비는 CharacterMovement의 이동 시간으로 계산하고 피해는 라운드에서 확정한다. */
UCLASS(ClassGroup=(Prototype01), meta=(BlueprintSpawnableComponent))
class SPACEPIRATE_API USP1SurvivalComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USP1SurvivalComponent();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Survival", meta=(ClampMin="0")) float SprintDrain = 20;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Survival", meta=(ClampMin="0")) float RecoveryDelay = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Survival", meta=(ClampMin="0")) float RecoveryRate = 25;
    UPROPERTY(ReplicatedUsing=OnRep_Life, BlueprintReadOnly) float Wounds = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) FSP1StaminaState ServerStamina;
    UPROPERTY(ReplicatedUsing=OnRep_Life, BlueprintReadOnly) bool bDead = false;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 DeathCount = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 DropBatches = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<APawn> SpectateTarget;
    UFUNCTION(BlueprintPure) float GetStamina() const;
    UFUNCTION(BlueprintPure) float GetMaximum() const { return SP1::StaminaMaximum(Wounds); }
    UFUNCTION(BlueprintPure) bool IsDead() const { return bDead || Wounds >= 100; }
    UFUNCTION(BlueprintPure) bool CanSprint() const;
    UFUNCTION(BlueprintPure) bool IsExhausted() const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void QueueWound(float Amount, FName HazardId);
    bool IsSimulationEnabled() const;
    void SimulateMovement(float Delta, bool bEligible, bool bSprintKey);
    void ReconcileMovement(const FSP1StaminaState& Authoritative, float ServerWounds);
    const FSP1StaminaState& GetMovementState() const { return MovementState; }
    void UpdateSpectator();
private:
    friend class USP1RoundComponent;
    void ApplyWound(float Amount, FName HazardId);
    void InitializeMaintenanceRevive();
    void ApplyDeathPresentation();
    UFUNCTION() void OnRep_Life();
    UFUNCTION() void TookDamage(AActor* Actor, float Damage, const UDamageType* Type, AController* Instigator, AActor* Causer);
    FSP1StaminaState MovementState;
    float LastAuthoritativeWounds = 0;
    TWeakObjectPtr<AActor> LastViewTarget;
    bool bLocalDeathApplied = false;
    bool bLocalControlInitialized = false;
};
