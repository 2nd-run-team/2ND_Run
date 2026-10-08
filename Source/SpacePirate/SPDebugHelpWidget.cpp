// 작업자: 김세훈 | 2026-10-08 | 프로토타입 디버그 도움말 신규 작성
// 변경 내용: 필요할 때만 여는 조작 안내, 호스트 전용 표시와 늘어난 항목의 페이지 이동을 구현한다.
// 작업자: 김세훈 | 2026-10-08 | 도움말 페이지 설정 변경
// 변경 내용: 페이지 안내에 이전은 쉼표, 다음은 마침표로 표시한다.

#include "SPDebugHelpWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const FLinearColor TextColor(0.88f, 0.96f, 1.0f);
    const FLinearColor MutedColor(0.57f, 0.68f, 0.76f);
    const FLinearColor AccentColor(0.35f, 0.80f, 1.0f);

    UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Parent, FName Name,
        const FText& Content, int32 Size, const FLinearColor& Color, bool bBold = false)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = Size;
        if (bBold) { Font.TypefaceFontName = TEXT("Bold"); }
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
        Text->SetAutoWrapText(true);
        Text->SetText(Content);
        Parent->AddChildToVerticalBox(Text)->SetPadding(FMargin(0.0f, 3.0f));
        return Text;
    }
}

USPDebugHelpWidget::USPDebugHelpWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    FSPDebugHelpEntry Damage;
    Damage.Category = NSLOCTEXT("SPDebugHelp", "StatusCategory", "체력 / 다운");
    Damage.Keys = FText::FromString(TEXT("F6"));
    Damage.Description = NSLOCTEXT("SPDebugHelp", "Damage", "자신의 체력 25 감소");
    Damage.bRequiresStatusDebugControls = true;
    HelpEntries.Add(Damage);

    FSPDebugHelpEntry Down = Damage;
    Down.Keys = FText::FromString(TEXT("F7"));
    Down.Description = NSLOCTEXT("SPDebugHelp", "Down", "자신을 다운 상태로 변경");
    HelpEntries.Add(Down);

    FSPDebugHelpEntry Reset = Damage;
    Reset.Category = NSLOCTEXT("SPDebugHelp", "OperationCategory", "작전");
    Reset.Keys = FText::FromString(TEXT("Shift + F7"));
    Reset.Description = NSLOCTEXT("SPDebugHelp", "Reset", "모든 플레이어의 체력·작전 실패·발각·경보 초기화");
    Reset.bHostOnly = true;
    HelpEntries.Add(Reset);
}

void USPDebugHelpWidget::Configure(FKey InToggleKey, bool bInStatusControlsEnabled)
{
    ToggleKey = InToggleKey;
    bStatusControlsEnabled = bInStatusControlsEnabled;
    RefreshPage();
}

TSharedRef<SWidget> USPDebugHelpWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) { BuildDefaultWidgetTree(); }
    return Super::RebuildWidget();
}

void USPDebugHelpWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
    RefreshPage();
}

void USPDebugHelpWidget::BuildDefaultWidgetTree()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DebugHelpRoot"));
    WidgetTree->RootWidget = Root;
    UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DebugHelpPanel"));
    Panel->SetBrushColor(FLinearColor(0.015f, 0.026f, 0.040f, 0.97f));
    Panel->SetPadding(FMargin(24.0f, 18.0f));
    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(1.0f, 0.5f));
    PanelSlot->SetAlignment(FVector2D(1.0f, 0.5f));
    PanelSlot->SetPosition(FVector2D(-28.0f, 0.0f));
    PanelSlot->SetAutoSize(true);

    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DebugHelpSize"));
    Size->SetWidthOverride(440.0f);
    Panel->SetContent(Size);
    UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DebugHelpContent"));
    Size->SetContent(Content);
    AddText(WidgetTree, Content, TEXT("HelpTitle"),
        NSLOCTEXT("SPDebugHelp", "Title", "프로토타입 디버그 조작"), 22, TextColor, true);
    AddText(WidgetTree, Content, TEXT("HelpSubtitle"),
        NSLOCTEXT("SPDebugHelp", "Subtitle", "도움말을 연 상태에서도 게임과 테스트 키는 동작합니다."), 12, MutedColor);
    HelpRows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("HelpRows"));
    Content->AddChildToVerticalBox(HelpRows)->SetPadding(FMargin(0.0f, 12.0f));
    PageText = AddText(WidgetTree, Content, TEXT("PageText"), FText::GetEmpty(), 12, MutedColor);
    CloseText = AddText(WidgetTree, Content, TEXT("CloseText"), FText::GetEmpty(), 13, AccentColor);
}

bool USPDebugHelpWidget::ShouldShowEntry(const FSPDebugHelpEntry& Entry) const
{
    return !Entry.bRequiresStatusDebugControls || bStatusControlsEnabled;
}

int32 USPDebugHelpWidget::GetPageCount() const
{
    int32 Count = 0;
    for (const FSPDebugHelpEntry& Entry : HelpEntries) { Count += ShouldShowEntry(Entry) ? 1 : 0; }
    return FMath::Max(1, FMath::DivideAndRoundUp(Count, EntriesPerPage));
}

void USPDebugHelpWidget::ChangePage(int32 Direction)
{
    PageIndex = FMath::Clamp(PageIndex + FMath::Sign(Direction), 0, GetPageCount() - 1);
    RefreshPage();
}

void USPDebugHelpWidget::RefreshPage()
{
    PageIndex = FMath::Clamp(PageIndex, 0, GetPageCount() - 1);
    if (HelpRows)
    {
        HelpRows->ClearChildren();
        const bool bIsHost = GetOwningPlayer() && GetOwningPlayer()->HasAuthority();
        int32 VisibleIndex = 0;
        FText PreviousCategory;
        for (const FSPDebugHelpEntry& Entry : HelpEntries)
        {
            if (!ShouldShowEntry(Entry)) { continue; }
            const int32 EntryIndex = VisibleIndex++;
            if (EntryIndex / EntriesPerPage != PageIndex) { continue; }
            if (!Entry.Category.IsEmpty() && !Entry.Category.EqualTo(PreviousCategory))
            {
                AddText(WidgetTree, HelpRows, NAME_None, Entry.Category, 12, AccentColor, true);
                PreviousCategory = Entry.Category;
            }
            FText Description = Entry.Description;
            if (Entry.bHostOnly)
            {
                Description = FText::Format(NSLOCTEXT("SPDebugHelp", "HostOnly", "{0} (호스트 전용)"), Description);
            }
            AddText(WidgetTree, HelpRows, NAME_None,
                FText::Format(NSLOCTEXT("SPDebugHelp", "Entry", "{0}  ·  {1}"), Entry.Keys, Description),
                15, Entry.bHostOnly && !bIsHost ? MutedColor : TextColor);
        }
        if (VisibleIndex == 0)
        {
            AddText(WidgetTree, HelpRows, NAME_None,
                NSLOCTEXT("SPDebugHelp", "Empty", "현재 활성화된 디버그 조작이 없습니다."), 15, MutedColor);
        }
    }
    if (PageText)
    {
        PageText->SetText(FText::Format(NSLOCTEXT("SPDebugHelp", "Pages", "{0} / {1} 페이지   ·   [,] 이전   [.] 다음"),
            FText::AsNumber(GetCurrentPage()), FText::AsNumber(GetPageCount())));
        PageText->SetVisibility(GetPageCount() > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (CloseText)
    {
        CloseText->SetText(FText::Format(NSLOCTEXT("SPDebugHelp", "Close", "{0}  도움말 닫기"), ToggleKey.GetDisplayName()));
    }
}
