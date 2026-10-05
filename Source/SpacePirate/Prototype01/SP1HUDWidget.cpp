// 작성자 : 임진혁
#include "Prototype01/SP1HUDWidget.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1CargoDefinition.h"
#include "Prototype01/SP1TransferCargo.h"
#include "Prototype01/SP1ExtractionZone.h"
#include "Prototype01/SP1SessionSubsystem.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "Prototype01/SP1Bulkhead.h"
#include "Prototype01/SP1CarRegion.h"
#include "Prototype01/SP1GravityCargo.h"
#include "Prototype01/SP1SecurityCamera.h"
#include "Prototype01/SP1LaserHazard.h"
#include "Prototype01/SP1MaintenanceStation.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/AudioComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    ESlateVisibility Visible(bool bShow) { return bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; }
    FString Clock(double Seconds)
    { const int32 S=FMath::Max(0,FMath::CeilToInt(Seconds)); return FString::Printf(TEXT("%02d:%02d"),S/60,S%60); }
    FString CarName(FName Car)
    { return Car.IsNone() ? TEXT("연결 통로") : FString::Printf(TEXT("%s번 칸"),*Car.ToString().Right(2)); }
    FString EndReason(const FString& Reason)
    {
        if (Reason==TEXT("MissionExpired")) return TEXT("임무 시간이 끝났습니다. 출발 중에도 임무 마감이 우선합니다.");
        if (Reason==TEXT("EmptyExtraction")) return TEXT("출발 순간 탈출 구역 안에 생존자가 없었습니다.");
        if (Reason==TEXT("NoConnectedSurvivors")) return TEXT("연결된 생존 팀원이 남아 있지 않습니다.");
        if (Reason==TEXT("Departed")) return TEXT("출발 순간 구역 안의 생존자가 탈출했습니다.");
        return TEXT("연결이 중단되었습니다. 이번 판의 정산은 없습니다.");
    }
    FString HoldReason(FName Reason)
    {
        if (Reason==TEXT("Completed")) return TEXT("[완료] 서버 확인 완료");
        if (Reason==TEXT("Released")) return TEXT("[취소] E를 놓았습니다");
        if (Reason==TEXT("TargetChanged")) return TEXT("[취소] 다른 대상을 조준했습니다");
        if (Reason==TEXT("TooFar")) return TEXT("[취소] 대상에서 너무 멀어졌습니다");
        if (Reason==TEXT("Blocked") || Reason==TEXT("Occluded")) return TEXT("[취소] 벽이나 물체에 가려졌습니다");
        if (Reason==TEXT("BadAim") || Reason==TEXT("AimLost")) return TEXT("[취소] 대상에서 시선이 벗어났습니다");
        if (Reason==TEXT("FocusLost") || Reason==TEXT("HeartbeatTimeout")) return TEXT("[취소] 입력 연결이 끊겼습니다");
        if (Reason==TEXT("Dead")) return TEXT("[취소] 사망하여 조작이 중단됐습니다");
        if (Reason==TEXT("NoTarget")) return TEXT("조작할 대상을 조준하세요");
        if (Reason==TEXT("EvaluationDebugBlocked")) return TEXT("평가판에서는 열차 속도·하늘 전환 키를 사용할 수 없습니다");
        if (Reason==TEXT("WindowExpired") || Reason==TEXT("MaintenanceClosed") || Reason==TEXT("SurvivorAdvanced")) return TEXT("[취소] 정비가 종료되었습니다");
        if (Reason==TEXT("AlreadyTransferred")) return TEXT("[완료] 다른 팀원이 이미 전송했습니다");
        if (Reason==TEXT("RoundEnded")) return TEXT("라운드가 종료되었습니다");
        return TEXT("[취소] 조작 조건을 벗어났습니다. 다시 조준해 E를 유지하세요.");
    }
    void ButtonLabel(UButton* Button,const FString& Label)
    { if (auto* Text=Button ? Cast<UTextBlock>(Button->GetContent()) : nullptr) Text->SetText(FText::FromString(Label)); }
}

void USP1HUDWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // 실제 배치·글꼴·색은 WBP의 Designer 트리에 저장한다. 런타임은 이름으로만 연결한다.
#define SP1_WIDGET(Type,Name) Name = WidgetTree->FindWidget<Type>(TEXT(#Name))
    SP1_WIDGET(UTextBlock,Status); SP1_WIDGET(UTextBlock,Target); SP1_WIDGET(UTextBlock,Reason);
    SP1_WIDGET(UProgressBar,Progress); SP1_WIDGET(UButton,ReadyButton); SP1_WIDGET(UButton,StartButton); SP1_WIDGET(UButton,RestartButton);
    SP1_WIDGET(UTextBlock,SurvivalText); SP1_WIDGET(UTextBlock,MechanicText); SP1_WIDGET(UProgressBar,SurvivalCapacity); SP1_WIDGET(UProgressBar,SurvivalStamina);
    SP1_WIDGET(UTextBlock,TeamText); SP1_WIDGET(UTextBlock,AlertText); SP1_WIDGET(UTextBlock,MenuTitle); SP1_WIDGET(UTextBlock,MenuBody);
    SP1_WIDGET(UTextBlock,PingText); SP1_WIDGET(UBorder,MenuPanel); SP1_WIDGET(UImage,TargetIcon); SP1_WIDGET(UButton,MuteButton);
#undef SP1_WIDGET
    if (!ensure(Status && Target && Progress && Reason && ReadyButton && StartButton && RestartButton)) return;
    ReadyButton->OnClicked.AddDynamic(this,&ThisClass::ReadyClicked);
    StartButton->OnClicked.AddDynamic(this,&ThisClass::StartClicked);
    RestartButton->OnClicked.AddDynamic(this,&ThisClass::RestartClicked);
    if (MuteButton) MuteButton->OnClicked.AddDynamic(this,&ThisClass::MuteClicked);
}
USP1InteractionComponent* USP1HUDWidget::Interaction() const
{ const auto* Pawn=GetOwningPlayerPawn(); return Pawn ? Pawn->FindComponentByClass<USP1InteractionComponent>() : nullptr; }
void USP1HUDWidget::NativeDestruct() { StopAudio(); Super::NativeDestruct(); }
void USP1HUDWidget::ReadyClicked() { if (auto* I=Interaction()) I->ToggleReady(); }
void USP1HUDWidget::StartClicked() { if (auto* I=Interaction()) I->RequestStart(); }
void USP1HUDWidget::RestartClicked()
{
    auto* Session=GetGameInstance() ? GetGameInstance()->GetSubsystem<USP1SessionSubsystem>() : nullptr;
    if (Session && Session->bAborted) Session->ReturnToPrototype();
    else if (auto* I=Interaction()) I->RequestRestart();
}
void USP1HUDWidget::MuteClicked()
{ if (auto* Session=GetGameInstance()->GetSubsystem<USP1SessionSubsystem>()) Session->ToggleFeedbackMute(); }

void USP1HUDWidget::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta);
    auto* Round=USP1RoundComponent::Find(this);
    auto* Session=GetGameInstance() ? GetGameInstance()->GetSubsystem<USP1SessionSubsystem>() : nullptr;
    if (Round && Session) Session->Remember(Round->State,GetClass());
    if (!Status || !MenuPanel) return;
    const bool bAborted=Session && Session->bAborted;
    const bool bLoading=(Session && Session->bLoading) || (!Round && !bAborted) || (Round && !Round->State.RunId.IsValid());
    const FSP1RunState Run=bAborted ? Session->LastState : Round ? Round->State : FSP1RunState();
    const double Time=Round ? Round->Now() : 0;
    const bool bResult=SP1::IsTerminal(Run.Phase), bReady=!bLoading && Run.Phase==ESP1Phase::Ready;
    const bool bMenu=bLoading || bReady || bResult;
    for (const TCHAR* Name : {TEXT("TeamPanel"),TEXT("DevicePanel"),TEXT("TargetPanel")})
        if (auto* Panel=WidgetTree->FindWidget<UBorder>(Name)) Panel->SetVisibility(Visible(!bMenu));
    auto* PC=GetOwningPlayer();
    if (!PC && GetGameInstance()) PC=GetGameInstance()->GetFirstLocalPlayerController();
    const bool bHost=PC && PC->HasAuthority();
    const APawn* Pawn=GetOwningPlayerPawn();
    const auto* Life=Pawn ? Pawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
    if (auto* Panel=WidgetTree->FindWidget<UBorder>(TEXT("LifePanel")))
        Panel->SetVisibility(Visible(Life && !bLoading));
    const auto* Region=Pawn ? ASP1CarRegion::FindAt(this,Pawn->GetActorLocation()) : nullptr;
    const auto* I=Interaction();
    const double Remaining=SP1::IsPlaying(Run.Phase) ? SP1::MissionRemaining(Run,Time) : Run.MissionLimit;
    DisplayedStatus=FString::Printf(TEXT("임무 %s%s  |  팀 미정산 %s\n%s / 총 10량"),*Clock(Remaining),Run.Maintenance.Phase==ESP1MaintenancePhase::Active ? TEXT(" · 정지") : TEXT(""),*FText::AsNumber(Run.TeamValue).ToString(),*CarName(Region ? Region->CarId : NAME_None));
    if (bResult) DisplayedStatus=FString::Printf(TEXT("라운드 종료  |  전송 %s\n최종 정산 %s"),*FText::AsNumber(Run.TeamValue).ToString(),*FText::AsNumber(Run.FinalValue).ToString());
    Status->SetText(FText::FromString(DisplayedStatus));
    Status->SetColorAndOpacity(Remaining<=30 && !bMenu ? FLinearColor(1,.42f,.24f) : FLinearColor::White);

    // 팀 정보는 서버 참가자 스냅샷을 쓴다. 이름 대신 고정 번호도 표시해 같은 모델을 구분한다.
    DisplayedTeam=TEXT("팀 상태");
    int32 Ready=0,Connected=0,Eligible=0;
    if (Round) for (int32 Index=0;Index<Round->Participants.Num();++Index)
    {
        const auto& P=Round->Participants[Index];
        Connected+=P.bConnected; Ready+=P.bConnected && P.bReady; Eligible+=SP1::CanRevive(P);
        const FString Name=P.PlayerState ? P.PlayerState->GetPlayerName().Left(12) : TEXT("연결 대기");
        const TCHAR* State=!P.bConnected ? TEXT("연결 끊김") : !P.bAlive ? TEXT("사망 · 관전") : bReady ? (P.bReady ? TEXT("준비 완료") : TEXT("준비 대기")) : TEXT("생존");
        DisplayedTeam+=FString::Printf(TEXT("\nP%d  %s  |  %s\n       %s · 상처 %.0f%s"),Index+1,*Name,State,*CarName(P.CarId),P.Wounds,P.bMaintenanceRevived ? TEXT(" · 부활 사용") : TEXT(""));
    }
    if (TeamText) { TeamText->SetText(FText::FromString(DisplayedTeam)); TeamText->SetVisibility(Visible(!bMenu)); }
    FString Pings=TEXT("MMB  위치/화물/위험 핑 · 30m · 8초");
    if (Round && !bResult) for (const auto& Ping : Round->Pings)
    {
        if (Ping.ExpiresAt<=Time) continue;
        const int32 Index=Round->Participants.IndexOfByPredicate([&](const auto& P){return P.PlayerId==Ping.PlayerId;});
        Pings+=FString::Printf(TEXT("\nP%d [%s] %.0fm · %.0f초"),Index+1,Ping.Kind==ESP1PingKind::Cargo ? TEXT("화물") : Ping.Kind==ESP1PingKind::Danger ? TEXT("위험 !") : TEXT("위치"),Pawn ? FVector::Distance(Pawn->GetActorLocation(),Ping.Location)/100 : 0,Ping.ExpiresAt-Time);
    }
    if (PingText) { PingText->SetText(FText::FromString(Pings)); PingText->SetVisibility(Visible(!bMenu)); }

    FString Mechanic=Region ? Region->HazardLabel.ToString() : TEXT("연결 통로 · 중력 유지");
    if (Run.Maintenance.Phase==ESP1MaintenancePhase::Active)
        Mechanic+=FString::Printf(TEXT("\n[정비] %s · 부활 가능 %d명\n임무만 정지 / 위험·확보는 계속\n6번 칸 진입 또는 출발 시 정비 종료"),*Clock(Run.Maintenance.Deadline-Time),Eligible);
    else if (Run.Maintenance.Phase==ESP1MaintenancePhase::Closed)
    {
        const FName Why=Run.Maintenance.CloseReason;
        Mechanic+=TEXT("\n[정비 종료] ")+FString(Why==TEXT("WindowExpired") ? TEXT("30초 만료") : Why==TEXT("SurvivorAdvanced") ? TEXT("팀원이 6번 이후로 전진") : Why==TEXT("DepartureStarted") ? TEXT("출발 승인") : TEXT("라운드 종료"));
    }
    if (Region && Region->CarId==TEXT("P01_C03")) for (TActorIterator<ASP1Bulkhead> It(GetWorld());It;++It)
    {
        auto Holder=[Round](const APawn* P){return Round && P ? Round->Participants.IndexOfByPredicate([&](const auto& Row){return Row.PlayerState==P->GetPlayerState();})+1 : 0;};
        Mechanic+=It->bOpen ? TEXT("\n[완료] 격벽 영구 개방") : FString::Printf(TEXT("\n패널 A: P%d / B: P%d\n우회: P%d · 함께 %.0f%%"),Holder(It->LeftHolder),Holder(It->RightHolder),Holder(It->BypassHolder),100*It->GetProgress(Time));
    }
    for (TActorIterator<ASP1GravityCargo> It(GetWorld());It;++It) if (Region && Region->CarId==It->CarId)
        Mechanic+=It->bTransferred ? TEXT("\n[무중력] 전송 완료 · 이 칸만 변경\nSpace 상승 / Shift 하강") : TEXT("\n[주의 S01] 확보하면 이 칸만 무중력\n바닥 먼저 / 높은 선반 +600");
    for (TActorIterator<ASP1SecurityCamera> It(GetWorld());It;++It) if (Pawn && Region && It->Region==Region)
    {
        const auto* E=It->Exposure.FindByPredicate([Pawn](const auto& Row){return Row.Player==Pawn->GetPlayerState();});
        Mechanic+=FString::Printf(TEXT("\n[감시] 방향 %.0f° · 노출 %.0f%%\n%s"),It->GetAim(Time).Yaw,E ? E->Amount*100 : 0,It->State.Phase==ESP1CameraPhase::Warning ? TEXT("! 경고: 지금 엄폐하세요") : It->State.Phase==ESP1CameraPhase::Cooldown ? TEXT("재사용 대기") : TEXT("탐색 중"));
    }
    if (MechanicText) { MechanicText->SetText(FText::FromString(Mechanic)); MechanicText->SetVisibility(Visible(!bMenu)); }
    if (Life && SurvivalText && SurvivalCapacity && SurvivalStamina)
    {
        SurvivalText->SetText(FText::FromString(FString::Printf(TEXT("%s  W %.0f | S %.0f / M %.0f\n청록: 스태미나 · 빈칸: 소진 · 빨강: 상처"),Life->IsDead() ? TEXT("[사망 · 팀원 관전]") : Life->IsExhausted() ? TEXT("[소진 · 걷기 가능]") : TEXT("[생존]"),Life->Wounds,Life->GetStamina(),Life->GetMaximum())));
        SurvivalCapacity->SetPercent(Life->GetMaximum()/100); SurvivalStamina->SetPercent(Life->GetStamina()/100);
    }
    FString Hint=TEXT("대상을 조준하고 E 유지\nWASD 이동 · 마우스 시점 · MMB 핑");
    UTexture2D* Icon=nullptr;
    if (I && !bMenu)
    {
        if (const auto* Cargo=Cast<ASP1TransferCargo>(I->FocusTarget);Cargo && Cargo->Definition)
        {
            Icon=Cargo->Definition->Icon;
            Hint=FString::Printf(TEXT("[E 유지] %s  +%d · %.1f초\n%s"),*Cargo->Definition->DisplayName.ToString(),Cargo->Definition->Value,Cargo->Definition->HoldSeconds,Cast<ASP1GravityCargo>(Cargo) ? TEXT("! 완료하면 이 객차만 무중력 / 화물은 고정") : TEXT("완료하면 자동 전송 · 손으로 운반하지 않음"));
        }
        else if (const auto* Exit=Cast<ASP1ExtractionZone>(I->FocusTarget)) Hint=FString::Printf(TEXT("[E 1초] %s\n10초 뒤 출발 · 출발 순간 구역 안 생존자 탑승"),*Exit->DepartureLabel.ToString());
        else if (Cast<ASP1MaintenanceStation>(I->FocusTarget)) Hint=FString::Printf(TEXT("[E 3초] 정비 부활 · 대상 %d명\nW50 / S50 · 인당 1회 · 소지품은 원래 위치"),Eligible);
        else if (const auto* Panel=Cast<ASP1Panel>(I->FocusTarget)) Hint=Panel->Kind==ESP1PanelKind::Bypass ? TEXT("[E 12초] 단독 우회\n혼자 남아도 격벽을 열 수 있습니다") : TEXT("[E 유지] 협동 패널\n서로 다른 두 명이 두 패널을 함께 2초");
    }
    Target->SetText(FText::FromString(Life && Life->IsDead() ? TEXT("관전 중 · 확보/장치/핑 조작 불가") : Hint));
    Target->SetVisibility(Visible(!bMenu)); Progress->SetVisibility(Visible(!bMenu));
    if (TargetIcon) { TargetIcon->SetBrushFromTexture(Icon); TargetIcon->SetVisibility(Visible(Icon && !bMenu)); }
    Progress->SetPercent(I ? I->GetProgress() : 0);
    if (I && LastReason!=I->LocalReason) { LastReason=I->LocalReason; ReasonVisibleUntil=Time+3.; }
    Reason->SetText(FText::FromString(I && !LastReason.IsNone() && Time<ReasonVisibleUntil && !bMenu ? HoldReason(LastReason) : TEXT("")));

    DisplayedAlert=TEXT("");
    if (!bMenu && Run.Phase==ESP1Phase::ExtractionCountdown)
        DisplayedAlert=FString::Printf(TEXT("[출발] %s  %s\n탑승 %d / 생존 %d · 구역 밖 %d"),*Run.DepartureLabel.ToString(),*Clock(Run.ExtractionDeadline-Time),Run.Aboard,Run.Living,FMath::Max(0,Run.Living-Run.Aboard));
    else if (!bMenu && Remaining<=30) DisplayedAlert=TEXT("! 임무 30초 이하 · 탈출에도 10초가 필요합니다");
    else if (!bMenu && Remaining<=60) DisplayedAlert=TEXT("! 임무 60초 이하 · 탈출 경로를 확인하세요");
    if (AlertText) AlertText->SetText(FText::FromString(DisplayedAlert));
    MenuPanel->SetVisibility(Visible(bMenu));
    FString Title=bLoading ? TEXT("열차 준비 중") : bReady ? TEXT("우주 열차 강탈") : Run.Phase==ESP1Phase::Succeeded ? TEXT("탈출 성공") : Run.Phase==ESP1Phase::Aborted ? TEXT("연결 중단 · 정산 없음") : TEXT("임무 실패");
    const TCHAR* Preset=Run.SettingsId==TEXT("TwoPlayer600") ? TEXT("2인 시작 설정") : Run.SettingsId==TEXT("Original600") ? TEXT("원안 비교 설정") : TEXT("4인 시험 설정");
    FString Body=bLoading ? TEXT("플레이어와 서버 상태를 불러오고 있습니다.") : bReady ? FString::Printf(TEXT("준비 %d / 연결 %d\n10량 · 화물 42개 · 총 가치 7,060\n이번 임무 %s / %s\nE 유지로 확보 → 자동 전송 → 구역에서 출발\n전원 준비 후 호스트가 시작합니다."),Ready,Connected,*Clock(Run.MissionLimit),Preset) : FString::Printf(TEXT("%s\n\n출발 장소: %s\n전송 %d  /  최종 정산 %d\n탈출 %d명  /  미탈출 %d명\n%s"),*EndReason(Run.Reason),Run.DepartureId.IsNone() ? TEXT("출발하지 않음") : *Run.DepartureLabel.ToString(),Run.TeamValue,Run.FinalValue,Run.Escaped,Run.LeftBehind,bAborted ? TEXT("새 로컬 호스트로 준비 화면을 열 수 있습니다.") : bHost ? TEXT("같은 맵에서 새 라운드를 시작할 수 있습니다.") : TEXT("호스트의 새 라운드를 기다리는 중입니다."));
    if (MenuTitle) MenuTitle->SetText(FText::FromString(Title));
    if (MenuBody) MenuBody->SetText(FText::FromString(Body));
    ReadyButton->SetVisibility(Visible(bReady)); StartButton->SetVisibility(Visible(bReady && bHost));
    StartButton->SetIsEnabled(Ready==Connected && Connected>0);
    RestartButton->SetVisibility(Visible(!bLoading && bResult && (bHost || bAborted)));
    ButtonLabel(RestartButton,bAborted ? TEXT("새 로컬 호스트로 준비 화면 열기") : TEXT("같은 맵 · 새 라운드"));
    ButtonLabel(MuteButton,Session && Session->bFeedbackMuted ? TEXT("피드백음 OFF · 켜기") : TEXT("피드백음 ON · 끄기"));
    bDrawWorldMarkers=!bMenu;
    if (!bLoading) UpdateAudio(Run,Time);
    // 다중 PIE에서도 비활성 창이 Slate 키보드/마우스 포커스를 빼앗지 않는다.
    // 창을 다시 선택하면 현재 Phase에 맞는 입력 모드를 한 번 복원한다.
    const auto* Player=PC ? PC->GetLocalPlayer() : nullptr;
    const auto* Viewport=Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
    const bool bForeground=Viewport && Viewport->IsForegroundWindow();
    const int32 DesiredPhase=bLoading ? -2 : static_cast<int32>(Run.Phase);
    if (PC) PC->bShowMouseCursor=bMenu;
    if (PC && bForeground && (InputPhase!=DesiredPhase || !bInputWasForeground))
    {
        InputPhase=DesiredPhase;
        if (bMenu) PC->SetInputMode(FInputModeGameAndUI()); else PC->SetInputMode(FInputModeGameOnly());
    }
    bInputWasForeground=bForeground;
    LastPhase=DesiredPhase;
}
