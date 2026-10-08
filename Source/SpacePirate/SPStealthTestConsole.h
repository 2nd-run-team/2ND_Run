#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPStealthTypes.h"
#include "SPStealthTestConsole.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USPInteractableComponent;
class UUserWidget;

UENUM(BlueprintType)
enum class ESPStealthTestCommand : uint8 { SubmitIncident, ReplayLastIncident, StartStage, EndStage, RestartStage, ObservedCrime };

/** Opt-in test-map actor. No RPC granting clients arbitrary incident authority. */
UCLASS()
class SPACEPIRATE_API ASPStealthTestDirector : public AActor
{
    GENERATED_BODY()
public:
    ASPStealthTestDirector();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Prototype") TSubclassOf<UUserWidget> StatusWidgetClass;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Stealth|Test") int32 FirstIdentityEffects = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Stealth|Test") int32 FirstAlarmEffects = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Stealth|Test") FString LastResult;
    void Execute(ESPStealthTestCommand Command, ESPStealthIncident Kind, APawn* User, FVector Location, FName Scope, FName Group,
        float ResponseRadius, int32 MaxResponders);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void OnIdentity(const FSPStealthIncidentRecord& Record);
    void OnAlarm(const FSPStealthIncidentRecord& Record);
    UPROPERTY(Transient) TArray<TObjectPtr<UUserWidget>> Widgets;
    UPROPERTY(Transient) FSPStealthIncidentRecord LastTestIncident;
};

/** E interaction provides a server-validated test button; box targeting is independent of its visual. */
UCLASS()
class SPACEPIRATE_API ASPStealthTestConsole : public AActor
{
    GENERATED_BODY()
public:
    ASPStealthTestConsole();
    virtual void OnConstruction(const FTransform& Transform) override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth|Test") TObjectPtr<UBoxComponent> InteractionVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth|Prototype") TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth|Prototype") TObjectPtr<UTextRenderComponent> Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth|Test") TObjectPtr<USPInteractableComponent> Interactable;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Test") ESPStealthTestCommand Command = ESPStealthTestCommand::SubmitIncident;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Test") ESPStealthIncident IncidentKind = ESPStealthIncident::LaserContact;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Test") FName IdentityScope = TEXT("StageSecurity");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Test") FName DispatchGroup = TEXT("StealthTest_Baseline");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Test") FVector IncidentLocation = FVector(550, -800, 0);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Test", meta=(ClampMin="0")) float ResponseRadius = 5000;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Test", meta=(ClampMin="0")) int32 MaxResponders = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Prototype") FText ButtonLabel;
protected:
    virtual void BeginPlay() override;
private:
    void Completed(APawn* User);
    bool CanUse(APawn* User);
};
