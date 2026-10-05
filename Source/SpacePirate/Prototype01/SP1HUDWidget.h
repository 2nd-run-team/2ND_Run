// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SP1HUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UButton;
class USoundBase;
class UAudioComponent;
class USP1InteractionComponent;
class UImage;
class UBorder;
class UTexture2D;

/** 전용 Widget BP의 이름 있는 위젯에 서버 상태와 로컬 진행률을 연결한다. */
UCLASS()
class SPACEPIRATE_API USP1HUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly) FString DisplayedStatus;
    UPROPERTY(BlueprintReadOnly) FString DisplayedTeam;
    UPROPERTY(BlueprintReadOnly) FString DisplayedAlert;
    UPROPERTY(BlueprintReadOnly) TArray<FName> AudioEvents;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TMap<FName,TObjectPtr<USoundBase>> Cues;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Icons") TObjectPtr<UTexture2D> LocationPingIcon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Icons") TObjectPtr<UTexture2D> CargoPingIcon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Icons") TObjectPtr<UTexture2D> DangerPingIcon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundBase> StartSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundBase> CancelSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundBase> SuccessSound;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01|Audio") TObjectPtr<USoundBase> FailureSound;
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry, float Delta) override;
    virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry,const FSlateRect& Culling,
        FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const override;
private:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Status;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Target;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> Reason;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> Progress;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> ReadyButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> StartButton;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UButton> RestartButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SurvivalText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> MechanicText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> SurvivalCapacity;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> SurvivalStamina;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TeamText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> AlertText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> MenuTitle;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> MenuBody;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PingText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> MenuPanel;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> TargetIcon;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> MuteButton;
    UPROPERTY() TObjectPtr<UAudioComponent> AlertAudio;
    FGuid ShownRunId;
    double PreviousRemaining = -1;
    double LastDepartureDeadline = 0;
    int32 LastCountdown = -1;
    bool bWarning60 = false;
    bool bWarning30 = false;
    bool bDrawWorldMarkers = false;
    void PlayCue(FName Cue, bool bAlert = false);
    void StopAudio();
    void UpdateAudio(const struct FSP1RunState& Run, double Time);
    int32 LastPhase = -1;
    int32 InputPhase = -1;
    bool bInputWasForeground = false;
    bool bWasHeld = false;
    FName LastReason;
    double ReasonVisibleUntil = 0;
    UPROPERTY() TObjectPtr<UAudioComponent> FeedbackAudio;
    void PlayFeedback(USoundBase* Sound, float Volume);
    USP1InteractionComponent* Interaction() const;
    UFUNCTION() void ReadyClicked();
    UFUNCTION() void StartClicked();
    UFUNCTION() void RestartClicked();
    UFUNCTION() void MuteClicked();
};
