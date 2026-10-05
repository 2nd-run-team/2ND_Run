// 작성자 : 임진혁
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Prototype01/SP1Types.h"
#include "SP1SessionSubsystem.generated.h"

class USP1HUDWidget;
class UNetDriver;
class UAudioComponent;

/** 서버가 사라지면 더 이상 복제할 수 없으므로 클라이언트에 지급 없는 Aborted 화면을 유지한다. */
UCLASS()
class SPACEPIRATE_API USP1SessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void Remember(const FSP1RunState& State, TSubclassOf<USP1HUDWidget> WidgetClass);
    UPROPERTY(BlueprintReadOnly) bool bAborted = false;
    UPROPERTY(BlueprintReadOnly) FSP1RunState LastState;
    UPROPERTY(BlueprintReadOnly) bool bFeedbackMuted = false;
    UPROPERTY(BlueprintReadOnly) bool bLoading = false;
    UFUNCTION(BlueprintCallable) void ToggleFeedbackMute();
    UFUNCTION(BlueprintCallable) void ReturnToPrototype();
    static bool CanPlayAudio(const UObject* Context);
    static void TrackAudio(const UObject* Context, UAudioComponent* Audio);
    bool ClaimAnnouncement(const FGuid& RunId,FName Cue);
private:
    FGuid AudioRun;
    TSet<FName> Announcements;
    TArray<TWeakObjectPtr<UAudioComponent>> PlayingAudio;
    void NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
    void RestoreAbortHUD(UWorld* World);
    FDelegateHandle FailureHandle;
    FDelegateHandle MapLoadedHandle;
    UPROPERTY() TSubclassOf<USP1HUDWidget> HUDClass;
    UPROPERTY() TObjectPtr<USP1HUDWidget> AbortHUD;
};
