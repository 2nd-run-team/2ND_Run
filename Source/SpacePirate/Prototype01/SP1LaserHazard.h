// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Prototype01/SP1SurvivalRules.h"
#include "SP1LaserHazard.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;

/** 서버 일정으로 작동하는 고정 레이저. VFX와 별개인 Box가 위험 영역이며 피해는 라운드 Tick에서 처리한다. */
UCLASS()
class SPACEPIRATE_API ASP1LaserHazard : public AActor
{
    GENERATED_BODY()
public:
    ASP1LaserHazard();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> DamageBounds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Emitter;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Receiver;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Indicator;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UNiagaraComponent>> Beams;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FName HazardId = TEXT("P01_C01_Laser01");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01", meta=(ClampMin="0.05")) float OnSeconds = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01", meta=(ClampMin="0.05")) float OffSeconds = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01", meta=(ClampMin="0")) float WarningSeconds = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01", meta=(ClampMin="0")) float WoundAmount = 20;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01", meta=(ClampMin="0.05")) float DamageInterval = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Visual") TObjectPtr<UNiagaraSystem> BeamSystem;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Visual") FLinearColor WarningColor = FLinearColor(1,0.55f,0.02f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Visual") FLinearColor ActiveColor = FLinearColor(1,0.02f,0.01f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundBase> WarningSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY(Replicated, BlueprintReadOnly) FGuid RunId;
    UPROPERTY(Replicated, BlueprintReadOnly) double CycleEpoch = -1;
    UPROPERTY(BlueprintReadOnly) ESP1LaserPhase DisplayedPhase = ESP1LaserPhase::Off;
    UFUNCTION(BlueprintPure) ESP1LaserPhase GetPhase() const;
    void StartCycle(const FGuid& NewRunId, double ServerTime);
    void EvaluateDamage(double ServerTime);
private:
    TMap<TWeakObjectPtr<APawn>, double> NextDamageAt;
    int32 LastVisualPhase = -1;
    UPROPERTY() TObjectPtr<UAudioComponent> WarningAudio;
    void UpdateBeamEndpoints();
};
