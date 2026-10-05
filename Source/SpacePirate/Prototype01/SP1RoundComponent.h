// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Prototype01/SP1Types.h"
#include "Prototype01/SP1RunRules.h"
#include "SP1RoundComponent.generated.h"

class ASP1TransferCargo;
class ASP1ExtractionZone;
class USP1InteractionComponent;
class APlayerController;
class USP1SurvivalComponent;
class ASP1MaintenanceStation;
class USP1RunSettings;
class USP1ScenarioDefinition;

/** 기존 Train/PlanetTrain GameState의 부모를 유지한 채 부착하는 서버 판정 경계. */
UCLASS(ClassGroup=(Prototype01), meta=(BlueprintSpawnableComponent))
class SPACEPIRATE_API USP1RoundComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USP1RoundComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01", meta=(ClampMin="1")) float MissionSeconds = 420;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01", meta=(ClampMin="0.1")) float DepartureSeconds = 10;
    UPROPERTY(Replicated, BlueprintReadOnly) FSP1RunState State;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<FSP1Participant> Participants;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<FSP1Ping> Pings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Telemetry") FString BuildIdentifier = TEXT("P05-local");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Telemetry") FName SettingsVersion = TEXT("Plan01-P05.1");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Telemetry") FName LayoutVariant = TEXT("A");
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<ASP1ExtractionZone> Extraction;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<ASP1MaintenanceStation> MaintenanceStation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Scenario") TObjectPtr<USP1ScenarioDefinition> Scenario;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Settings") TObjectPtr<USP1RunSettings> TwoPlayerSettings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Settings") TObjectPtr<USP1RunSettings> FourPlayerSettings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Settings") TObjectPtr<USP1RunSettings> SettingsOverride;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Settings") bool bUseSettingsAssets = true;

    static USP1RoundComponent* Find(const UObject* Context);
    double Now() const;
    bool CanInteract(const APawn* Pawn) const;
    bool CanUseExtraction(const ASP1ExtractionZone* Zone) const;
    bool CanUseMaintenance() const;
    bool IsHazardProtected(const APawn* Pawn) const;
    void SetReady(APlayerController* PC, const FGuid& RunId, bool bReady);
    void StartRun(APlayerController* PC, const FGuid& RunId);
    void RestartRun(APlayerController* PC, const FGuid& RunId);
    bool CompleteTarget(USP1InteractionComponent* Source);
    void QueueDamage(USP1SurvivalComponent* Target, float Amount, FName HazardId);
    void RequestPing(USP1InteractionComponent* Source, const FGuid& RunId);
    void LogEvent(const TCHAR* Event, const USP1InteractionComponent* Source = nullptr,
        FName CargoId = NAME_None, const FString& Reason = FString(), int32 Value = 0,
        FName ExplicitCar = NAME_None, int32 ExplicitPlayer = INDEX_NONE) const;
private:
    // 객차 방문 기록은 참가자별로 유지한다. 팀 체류/첫 이탈 잔여 가치는 로그 검사 도구가 별도로 집계한다.
    TMap<int32,FName> PreviousCars;
    TMap<int32,double> NextPingAt;
    int32 NextPingId = 0;
    mutable int32 LogSequence = 0;
    void UpdatePresentationState();
    void CloseCarVisits(const FString& Reason);
    int32 RemainingCarValue(FName CarId) const;
    FSP1RunLedger Ledger;
    struct FPendingDamage { TWeakObjectPtr<USP1SurvivalComponent> Target; float Amount; FName HazardId; };
    TArray<FPendingDamage> PendingDamage;
    bool bConfigurationValid = false;
    UPROPERTY() TArray<TObjectPtr<ASP1TransferCargo>> Cargo;
    UPROPERTY() TArray<TObjectPtr<ASP1ExtractionZone>> Exits;
    void ValidateScenario();
    void UpdateMaintenance(double Time);
    void EndMaintenance(double Time, FName Reason);
    bool ReviveParticipants(USP1InteractionComponent* Source);
    void SelectRunSettings();
    void ResetRun(bool bResetPawns);
    void RefreshParticipants();
    void Finish(ESP1Phase Result, const FString& Reason);
    APlayerController* FindController(int32 PlayerId) const;
};
