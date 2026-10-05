// 작성자 : 임진혁
#include "Prototype01/SP1SessionSubsystem.h"
#include "Prototype01/SP1HUDWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectGlobals.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void USP1SessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (GEngine) FailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::NetworkFailed);
    MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::RestoreAbortHUD);
}
void USP1SessionSubsystem::Deinitialize()
{
    if (GEngine) GEngine->OnNetworkFailure().Remove(FailureHandle);
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle);
    if (AbortHUD) AbortHUD->RemoveFromParent();
    Super::Deinitialize();
}
void USP1SessionSubsystem::Remember(const FSP1RunState& State, TSubclassOf<USP1HUDWidget> WidgetClass)
{
    if (!State.RunId.IsValid() || (bAborted && State.RunId == LastState.RunId)) return;
    if (State.RunId != LastState.RunId && AbortHUD) { AbortHUD->RemoveFromParent(); AbortHUD = nullptr; }
    bAborted = false;
    bLoading = false;
    LastState = State;
    HUDClass = WidgetClass;
}

bool USP1SessionSubsystem::CanPlayAudio(const UObject* Context)
{
    const auto* World = Context ? Context->GetWorld() : nullptr;
    const auto* GI = World ? World->GetGameInstance() : nullptr;
    const auto* Session = GI ? GI->GetSubsystem<USP1SessionSubsystem>() : nullptr;
    return Session && !Session->bFeedbackMuted && !Session->bLoading && !Session->bAborted;
}
void USP1SessionSubsystem::TrackAudio(const UObject* Context,UAudioComponent* Audio)
{
    auto* World = Context ? Context->GetWorld() : nullptr;
    auto* Session = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<USP1SessionSubsystem>() : nullptr;
    if (!Session || !Audio) return;
    Session->PlayingAudio.RemoveAll([](const auto& A){return !A.IsValid() || !A->IsPlaying();});
    Session->PlayingAudio.Add(Audio);
}
void USP1SessionSubsystem::ToggleFeedbackMute()
{
    bFeedbackMuted = !bFeedbackMuted;
    if (bFeedbackMuted) for (auto& Audio : PlayingAudio) if (Audio.IsValid()) Audio->Stop();
}
bool USP1SessionSubsystem::ClaimAnnouncement(const FGuid& RunId,FName Cue)
{
    if (AudioRun!=RunId) { AudioRun=RunId; Announcements.Reset(); }
    if (Announcements.Contains(Cue)) return false;
    Announcements.Add(Cue); return true;
}
void USP1SessionSubsystem::ReturnToPrototype()
{
    if (!bAborted || bLoading || !GetWorld()) return;
    bLoading = true;
    for (auto& Audio : PlayingAudio) if (Audio.IsValid()) Audio->Stop();
    // 로딩 화면을 한 프레임 이상 그린 뒤 새 로컬 호스트의 준비 화면으로 이동한다. 이전 연결에 재가입하지 않는다.
    FTimerHandle Handle;
    GetWorld()->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateWeakLambda(this,[this]
    {
        bAborted=false; LastState=FSP1RunState();
        UGameplayStatics::OpenLevel(GetGameInstance(),TEXT("/Game/SpacePirate/Maps/Lvl_SPPrototype01"),true,TEXT("listen"));
    }),0.2f,false);
}
void USP1SessionSubsystem::NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error)
{
    if (!World || World->GetGameInstance() != GetGameInstance() || World->GetNetMode() != NM_Client
        || !LastState.RunId.IsValid() || bAborted || SP1::IsTerminal(LastState.Phase)) return;
    bAborted = true;
    for (auto& Audio : PlayingAudio) if (Audio.IsValid()) Audio->Stop();
    LastState.Phase = ESP1Phase::Aborted;
    LastState.FinalValue = 0;
    LastState.Escaped = 0;
    LastState.LeftBehind = FMath::Max(LastState.LeftBehind, LastState.Living);
    LastState.Reason = TEXT("Host connection lost: ") + Error;
    UE_LOG(LogTemp, Warning, TEXT("[SP1] ClientAborted RunId=%s FinalValue=0 Reason=%s"), *LastState.RunId.ToString(), *Error);
    RestoreAbortHUD(World);
}

void USP1SessionSubsystem::RestoreAbortHUD(UWorld* World)
{
    if (!bAborted || !World || World->GetGameInstance() != GetGameInstance() || !HUDClass) return;
    // 연결 실패 직후 UE가 기본 맵으로 이동하면 기존 뷰포트 위젯은 제거된다.
    // GI에 남긴 결과로 새 월드에 한 개만 다시 만든다. 기존 기본 맵 자산은 수정하지 않는다.
    if (AbortHUD) AbortHUD->RemoveFromParent();
    AbortHUD = CreateWidget<USP1HUDWidget>(GetGameInstance(), HUDClass);
    if (AbortHUD) AbortHUD->AddToViewport(100);
    if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController(World))
    {
        PC->SetIgnoreMoveInput(true);
        PC->SetIgnoreLookInput(true);
    }
}
