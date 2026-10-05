// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Prototype01/SP1Types.h"
#include "SP1InteractionComponent.generated.h"

class UEnhancedInputComponent;
class UInputAction;
class USP1RoundComponent;
class USP1HUDWidget;
class APlayerController;
class UInputComponent;

/** 전용 BP Pawn에 이 컴포넌트를 추가하는 것이 E 홀드 입력의 opt-in이다. */
UCLASS(ClassGroup=(Prototype01), meta=(BlueprintSpawnableComponent))
class SPACEPIRATE_API USP1InteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USP1InteractionComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void BindInput(UEnhancedInputComponent* Input, UInputAction* Action);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Hold") float StartDistance = 250;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Hold") float MaintainDistance = 300;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Hold") float AimAngleDegrees = 20;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Hold") float AimGraceSeconds = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Hold") float HeartbeatInterval = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Hold") float HeartbeatTimeout = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|UI") TSubclassOf<USP1HUDWidget> HUDClass;
    UPROPERTY(ReplicatedUsing=OnRep_Attempt, BlueprintReadOnly) FSP1Attempt Attempt;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> FocusTarget;
    UPROPERTY(BlueprintReadOnly) FName LocalReason;
    UPROPERTY(BlueprintReadOnly) bool bLocalHeld = false;

    UFUNCTION(BlueprintCallable) void BeginHold();
    UFUNCTION(BlueprintCallable) void HoldTriggered();
    UFUNCTION(BlueprintCallable) void EndHold();
    UFUNCTION(BlueprintCallable) void ToggleReady();
    UFUNCTION(BlueprintCallable) void RequestStart();
    UFUNCTION(BlueprintCallable) void RequestRestart();
    UFUNCTION(BlueprintCallable) void RequestPing();
    UFUNCTION(BlueprintPure) float GetProgress() const;
    UFUNCTION(BlueprintPure) bool IsAlive() const;
    bool IsServerHolding() const { return GetOwner()->HasAuthority() && Attempt.bActive; }
    APlayerController* GetPlayerController() const;
    void EvaluateOnServer(double Now);
    bool ValidateDeviceHold(double Now, bool bStrictAim);
    void CancelOnServer(FName Reason);
    void ResetForRun();

    // 클라이언트가 보낸 값은 시도 식별자뿐이다. 완료·시간·가치 RPC는 제공하지 않는다.
    UFUNCTION(Server, Reliable) void ServerStart(FGuid RunId, int32 RequestId, AActor* Target);
    UFUNCTION(Server, Reliable) void ServerCancel(FGuid RunId, int32 RequestId, int32 AttemptId);
    UFUNCTION(Server, Unreliable) void ServerHeartbeat(FGuid RunId, int32 RequestId, int32 AttemptId);
    UFUNCTION(Server, Reliable) void ServerReady(FGuid RunId, bool bReady);
    UFUNCTION(Server, Reliable) void ServerStartRun(FGuid RunId);
    UFUNCTION(Server, Reliable) void ServerRestartRun(FGuid RunId);
    UFUNCTION(Server, Reliable) void ServerPing(FGuid RunId);
private:
    void InstallEvaluationInput();
    void BlockDebugKey();
    UPROPERTY() TObjectPtr<UInputComponent> EvaluationInput;
    TWeakObjectPtr<APlayerController> InputController;
    UFUNCTION() void OnRep_Attempt();
    FName ValidateTarget(AActor* Target, bool bStarting, bool bStrictAim, double Now);
    AActor* FindFocus() const;
    void SetMovementRequest(bool bRequested);
    void OnAppDeactivated();
    bool IsLocal() const;
    int32 LocalRequestId = 0;
    int32 LastServerRequestId = 0;
    int32 NextAttemptId = 0;
    FGuid LocalRunId;
    UPROPERTY() TObjectPtr<AActor> LocalTarget;
    UPROPERTY() TObjectPtr<USP1HUDWidget> HUD;
    double LastTriggerAt = -100;
    double LastPulseAt = -100;
    double LastServerPulseAt = 0;
    double LastValidAimAt = 0;
    FDelegateHandle DeactivateHandle;
};
