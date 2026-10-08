// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 신규 작성
// 변경 내용: 소유자의 체력 숫자·다운·구조 진행·작전 실패를 표시하는 임시 UMG HUD를 구현한다.
// 작업자: 김세훈 | 2026-10-08 | 디버그 도움말 분리
// 변경 내용: 기본 HUD에서 테스트 조작법을 제거하고 기존 BP의 HintText도 숨긴다.
// 작업자: 김세훈 | 2026-10-08 | 도움말 키 안내
// 변경 내용: 체력 패널 바로 위에 실제 도움말 키를 옅게 표시하고 도움말을 열면 안내를 숨긴다.

#include "SPPlayerStatusHUDWidget.h"

#include "SPGameState.h"
#include "SPInteractorComponent.h"
#include "SPPlayerCharacter.h"
#include "SPPlayerStatusComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const FLinearColor HealthyColor(0.88f, 0.96f, 1.0f);
    const FLinearColor WarningColor(1.0f, 0.72f, 0.20f);
    const FLinearColor DownedColor(1.0f, 0.22f, 0.19f);
    const FLinearColor QuietColor(0.57f, 0.68f, 0.76f);

    int32 DisplayHealth(float Health)
    {
        // 100 * 0.3f의 미세 오차를 31로 올리지 않되, 남은 양수 체력은 최소 1로 표시한다.
        return Health > 0.0f ? FMath::Max(1, FMath::CeilToInt(Health - UE_KINDA_SMALL_NUMBER)) : 0;
    }

    void SetTextStyle(UTextBlock* Text, int32 FontSize, const FLinearColor& Color, bool bBold = false)
    {
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        if (bBold)
        {
            Font.TypefaceFontName = TEXT("Bold");
        }
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
    }
}

TSharedRef<SWidget> USPPlayerStatusHUDWidget::RebuildWidget()
{
    // Designer에 루트가 있는 BP는 그 레이아웃을 보존한다.
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildDefaultWidgetTree();
    }
    return Super::RebuildWidget();
}

void USPPlayerStatusHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    RefreshDisplay();
}

void USPPlayerStatusHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshDisplay();
}

USPPlayerStatusComponent* USPPlayerStatusHUDWidget::GetDisplayedStatusComponent() const
{
    const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(GetOwningPlayerPawn());
    return Player ? Player->GetStatusComponent() : nullptr;
}

void USPPlayerStatusHUDWidget::BuildDefaultWidgetTree()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StatusHUDRoot"));
    WidgetTree->RootWidget = Root;

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusLayout"));
    UCanvasPanelSlot* LayoutSlot = Root->AddChildToCanvas(Layout);
    LayoutSlot->SetAnchors(FAnchors(0.0f, 1.0f));
    LayoutSlot->SetAlignment(FVector2D(0.0f, 1.0f));
    LayoutSlot->SetPosition(FVector2D(28.0f, -28.0f));
    LayoutSlot->SetAutoSize(true);

    HelpHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HelpHintText"));
    SetTextStyle(HelpHintText, 13, FLinearColor(0.68f, 0.75f, 0.81f, 0.70f));
    HelpHintText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
    HelpHintText->SetShadowOffset(FVector2D(1.0f, 1.0f));
    Layout->AddChildToVerticalBox(HelpHintText)->SetPadding(FMargin(20.0f, 0.0f, 0.0f, 8.0f));

    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusPanel"));
    Panel->SetBrushColor(FLinearColor(0.015f, 0.026f, 0.040f, 0.90f));
    Panel->SetPadding(FMargin(20.0f, 16.0f));
    Layout->AddChildToVerticalBox(Panel);

    USizeBox* Sizing = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatusPanelSize"));
    Sizing->SetWidthOverride(340.0f);
    Panel->SetContent(Sizing);
    UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusRows"));
    Sizing->SetContent(Rows);

    auto AddRow = [this, Rows](FName Name, int32 FontSize, const FLinearColor& Color, bool bBold = false)
    {
        UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        SetTextStyle(Text, FontSize, Color, bBold);
        Text->SetAutoWrapText(true);
        Rows->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.0f, 2.0f));
        return Text;
    };

    UTextBlock* Title = AddRow(TEXT("StatusTitle"), 12, QuietColor, true);
    Title->SetText(NSLOCTEXT("PlayerStatusHUD", "Title", "PLAYER VITALS  /  PROTOTYPE"));
    HealthText = AddRow(TEXT("HealthText"), 32, HealthyColor, true);
    StateText = AddRow(TEXT("StateText"), 18, HealthyColor, true);
    RescueText = AddRow(TEXT("RescueText"), 15, HealthyColor);
    TeamText = AddRow(TEXT("TeamText"), 13, QuietColor);

    FailureText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FailureText"));
    SetTextStyle(FailureText, 28, DownedColor, true);
    FailureText->SetJustification(ETextJustify::Center);
    FailureText->SetShadowColorAndOpacity(FLinearColor::Black);
    FailureText->SetShadowOffset(FVector2D(2.0f, 2.0f));
    FailureText->SetText(NSLOCTEXT("PlayerStatusHUD", "OperationFailed", "OPERATION FAILED\nAll players are down"));
    UCanvasPanelSlot* FailureSlot = Root->AddChildToCanvas(FailureText);
    FailureSlot->SetAnchors(FAnchors(0.5f, 0.0f));
    FailureSlot->SetAlignment(FVector2D(0.5f, 0.0f));
    FailureSlot->SetPosition(FVector2D(0.0f, 36.0f));
    FailureSlot->SetAutoSize(true);
}

void USPPlayerStatusHUDWidget::RefreshDisplay()
{
    const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(GetOwningPlayerPawn());
    const USPPlayerStatusComponent* Life = GetDisplayedStatusComponent();
    const ASPGameState* OperationState = GetWorld() ? GetWorld()->GetGameState<ASPGameState>() : nullptr;
    const bool bDowned = Life && Life->IsDowned();

    if (HealthText)
    {
        HealthText->SetText(Life
            ? FText::Format(NSLOCTEXT("PlayerStatusHUD", "Health", "HP {0} / {1}"),
                FText::AsNumber(DisplayHealth(Life->GetHealth())),
                FText::AsNumber(DisplayHealth(Life->GetMaxHealth())))
            : NSLOCTEXT("PlayerStatusHUD", "NoHealth", "HP -- / --"));
        HealthText->SetColorAndOpacity(FSlateColor(bDowned ? DownedColor
            : Life && Life->GetHealthPercent() <= 0.3f ? WarningColor : HealthyColor));
    }

    if (StateText)
    {
        StateText->SetText(!Life ? NSLOCTEXT("PlayerStatusHUD", "NoPlayer", "WAITING FOR PLAYER")
            : bDowned ? NSLOCTEXT("PlayerStatusHUD", "Downed", "DOWNED")
            : NSLOCTEXT("PlayerStatusHUD", "Active", "ACTIVE"));
        StateText->SetColorAndOpacity(FSlateColor(bDowned ? DownedColor : HealthyColor));
    }

    if (RescueText)
    {
        FText RescueMessage;
        if (bDowned)
        {
            const float Progress = Life->GetReviveProgress();
            RescueMessage = Progress > 0.0f
                ? FText::Format(NSLOCTEXT("PlayerStatusHUD", "BeingRevived", "BEING REVIVED: {0}%"),
                    FText::AsNumber(FMath::FloorToInt(Progress * 100.0f)))
                : NSLOCTEXT("PlayerStatusHUD", "WaitForRescue", "A teammate can hold E to revive you.");
        }
        else if (Player && Player->GetInteractor() && Player->GetInteractor()->IsHolding())
        {
            RescueMessage = FText::Format(NSLOCTEXT("PlayerStatusHUD", "Holding", "{0}: {1}%"),
                Player->GetInteractor()->GetHoldPrompt(),
                FText::AsNumber(FMath::FloorToInt(Player->GetHoldProgress() * 100.0f)));
        }
        RescueText->SetText(RescueMessage);
        RescueText->SetVisibility(RescueMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }

    if (TeamText)
    {
        TeamText->SetText(OperationState
            ? FText::Format(NSLOCTEXT("PlayerStatusHUD", "Team", "ACTIVE CREW  {0} / {1}"),
                FText::AsNumber(OperationState->GetActivePlayerCount()),
                FText::AsNumber(OperationState->GetParticipatingPlayerCount())) : FText::GetEmpty());
    }

    if (HintText)
    {
        HintText->SetText(FText::GetEmpty());
        HintText->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (HelpHintText)
    {
        const bool bShowHelpHint = Player && Player->IsDebugHelpAvailable() && !Player->IsDebugHelpOpen();
        HelpHintText->SetText(bShowHelpHint
            ? FText::Format(NSLOCTEXT("PlayerStatusHUD", "OpenDebugHelp", "{0}  디버그 도움말 열기"),
                Player->GetDebugHelpKey().GetDisplayName())
            : FText::GetEmpty());
        HelpHintText->SetVisibility(bShowHelpHint ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (FailureText)
    {
        FailureText->SetText(NSLOCTEXT("PlayerStatusHUD", "OperationFailed", "OPERATION FAILED\nAll players are down"));
        FailureText->SetVisibility(OperationState && OperationState->IsOperationFailed()
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}
