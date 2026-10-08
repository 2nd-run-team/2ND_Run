#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPStealthStatusWidget.generated.h"
class UTextBlock;

/** Temporary read-only HUD; all displayed state is read from replicated PS/GS components. */
UCLASS()
class SPACEPIRATE_API USPStealthStatusWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Stealth|UI") FText GetStatusText() const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
    float RefreshRemaining = 0;
};
