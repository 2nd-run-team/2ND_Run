#include "SPStealthStatusWidget.h"
#include "SPStealthGameStateComponent.h"
#include "SPStealthPlayerStateComponent.h"
#include "SPStealthTestConsole.h"
#include "SPStealthActivityComponent.h"
#include "SPRestrictedArea.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"

void USPStealthStatusWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // A BP subclass can replace the entire widget through the director's editable class reference.
    auto* Border = WidgetTree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.035f, 0.88f));
    Border->SetPadding(FMargin(12));
    StatusText = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo Font = StatusText->GetFont();
    Font.Size = 14;
    StatusText->SetFont(Font);
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.95f, 1)));
    Border->SetContent(StatusText);
    WidgetTree->RootWidget = Border;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    StatusText->SetText(GetStatusText());
}
FText USPStealthStatusWidget::GetStatusText() const
{
    const auto* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    const auto* Security = GS ? GS->FindComponentByClass<USPStealthGameStateComponent>() : nullptr;
    if (!Security) { return FText::FromString(TEXT("잠입 시험 | 서버 상태를 받는 중...")); }
    const auto Alarm = Security->GetAlarmState();
    FString Text = FString::Printf(TEXT("잠입 시험  |  %s  |  전체 경보: %s\n시험 회차: %s\n"),
        Alarm.bStageActive ? TEXT("진행 중") : TEXT("종료됨"), Alarm.bGlobalAlarm ? TEXT("발생") : TEXT("없음"),
        *Alarm.StageId.ToString().Left(8));
    TArray<APlayerState*> Players;
    for (APlayerState* PS : GS->PlayerArray) { if (IsValid(PS) && !PS->IsOnlyASpectator()) { Players.Add(PS); } }
    Players.Sort([](const APlayerState& A, const APlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
    int32 PlayerNumber = 0;
    for (const auto* PS : Players)
    {
        const auto* Identity = PS->FindComponentByClass<USPStealthPlayerStateComponent>();
        const bool bReady = Identity && Identity->GetIdentityState().StageId == Alarm.StageId;
        const auto* Activity=PS->GetPawn()?PS->GetPawn()->FindComponentByClass<USPStealthActivityComponent>():nullptr;
        const FString Zone=!Activity?TEXT("동기화 중"):Activity->GetCurrentArea()?TEXT("제한 구역"):TEXT("일반 구역");
        Text += FString::Printf(TEXT("플레이어 %d%s: %s | %s\n"), ++PlayerNumber,
            GetOwningPlayer() && GetOwningPlayer()->PlayerState == PS ? TEXT(" [나]") : TEXT(""),
            !bReady ? TEXT("동기화 중") : Identity->IsIdentified() ? TEXT("신원 발각") : TEXT("미발각"),*Zone);
    }
    for (TActorIterator<ASPStealthTestDirector> It(GetWorld()); It; ++It)
    {
        Text += FString::Printf(TEXT("최초 발생: 발각 %d회 / 경보 %d회\n%s\n"), It->FirstIdentityEffects, It->FirstAlarmEffects, *It->LastResult);
        break;
    }
    Text += TEXT("권장 순서: 01 → 03 → 04 → 02 → 05\n06·07: 강제 사건 / 08: 보안 초기화\n상자를 조준하고 E. '유지' 표시는 길게 누르기.");
    return FText::FromString(Text);
}
void USPStealthStatusWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    RefreshRemaining -= DeltaSeconds;
    if (StatusText && RefreshRemaining <= 0) { RefreshRemaining = 0.1f; StatusText->SetText(GetStatusText()); }
}

