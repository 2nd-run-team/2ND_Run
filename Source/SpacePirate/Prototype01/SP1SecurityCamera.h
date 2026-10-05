// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SP1SecurityCamera.generated.h"
class ASP1CarRegion;
class UStaticMeshComponent;
class UTextRenderComponent;
class USpotLightComponent;
class APlayerState;
class USoundBase;
class USoundAttenuation;
class UAudioComponent;
UENUM(BlueprintType)
enum class ESP1CameraPhase : uint8 { Scanning, Warning, Cooldown };
USTRUCT(BlueprintType)
struct FSP1CameraState
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid RunId;
    UPROPERTY(BlueprintReadOnly) ESP1CameraPhase Phase = ESP1CameraPhase::Scanning;
    UPROPERTY(BlueprintReadOnly) double Epoch = 0;
    UPROPERTY(BlueprintReadOnly) double Deadline = 0;
    UPROPERTY(BlueprintReadOnly) float LockedYaw = 0;
};
USTRUCT(BlueprintType)
struct FSP1Exposure
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> Player;
    UPROPERTY(BlueprintReadOnly) float Amount = 0;
};
/** 회전/예고는 서버 시간으로 표현하고, 판정은 Round의 피해 단계에서만 실행한다. */
UCLASS()
class SPACEPIRATE_API ASP1SecurityCamera : public AActor
{
    GENERATED_BODY()
public:
    ASP1SecurityCamera();
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> Pivot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USpotLightComponent> DirectionLight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> Indicator;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") TObjectPtr<ASP1CarRegion> Region;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FName HazardId = TEXT("P01_C05_Camera01");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float Range = 800;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float FullAngle = 60;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float ExposureSeconds = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float DecayPerSecond = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float WarningSeconds = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float CooldownSeconds = 6;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float WoundAmount = 20;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float SweepSeconds = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float SweepHalfAngle = 45;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float AimPitch = -10;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundBase> WarningSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY(Replicated, BlueprintReadOnly) FSP1CameraState State;
    UPROPERTY(Replicated, BlueprintReadOnly) TArray<FSP1Exposure> Exposure;
    UFUNCTION(BlueprintPure) FRotator GetAim(double ServerTime) const;
    UFUNCTION(BlueprintPure) bool CanSee(const APawn* Pawn, double ServerTime) const;
    void ResetForRun(FGuid RunId, double Time);
    void EvaluateOnServer(double Time, float Delta);
private:
    bool bWasWarning = false;
    UPROPERTY() TObjectPtr<UAudioComponent> WarningAudio;
};
