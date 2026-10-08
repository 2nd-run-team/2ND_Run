#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPRestrictedArea.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

/** Shared spatial rule. Never attached to or derived from a patrol route at runtime. */
UCLASS()
class SPACEPIRATE_API ASPRestrictedArea : public AActor
{
    GENERATED_BODY()
public:
    ASPRestrictedArea();
    virtual void Tick(float DeltaSeconds) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth|Area") TObjectPtr<UBoxComponent> Volume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Stealth|Prototype") TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Stealth|Area") bool bRestrictedEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Stealth|Area") FText AreaName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Debug") bool bDrawBounds = false;
    UFUNCTION(BlueprintPure, Category="Stealth|Area") bool ContainsLocation(FVector Location) const;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Area") void SetRestrictedEnabled(bool bEnabled);
    UFUNCTION(BlueprintPure, Category="Stealth|Area", meta=(WorldContext="WorldContextObject"))
    static ASPRestrictedArea* FindAtLocation(const UObject* WorldContextObject, FVector Location);
};
