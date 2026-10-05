// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SP1TransferCargo.generated.h"

class USP1CargoDefinition;
class UAudioComponent;
class UStaticMeshComponent;

UCLASS()
class SPACEPIRATE_API ASP1TransferCargo : public AActor
{
    GENERATED_BODY()
public:
    ASP1TransferCargo();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float Delta) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FName CargoId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FName CarId = TEXT("P01_C01");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") TObjectPtr<USP1CargoDefinition> Definition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(ReplicatedUsing=OnRep_Transferred, BlueprintReadOnly) bool bTransferred = false;
    UFUNCTION(BlueprintPure) FVector GetInteractionPoint() const;
    virtual void SetTransferred(bool bNewTransferred);
protected:
    UPROPERTY() TObjectPtr<UAudioComponent> TransferAudio;
    UFUNCTION() virtual void OnRep_Transferred();
};
