// 작성자 : 임진혁
#include "Prototype01/Editor/SP1EditorLibrary.h"
#if WITH_EDITOR
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#endif

bool USP1EditorLibrary::BuildHUDTemplate(UObject* Blueprint)
{
#if WITH_EDITOR
    auto* BP = Cast<UWidgetBlueprint>(Blueprint);
    if (!BP || BP->GetPathName() != TEXT("/Game/SpacePirate/Prototype01/UI/WBP_SP1HUD.WBP_SP1HUD")) return false;
    UWidgetTree* Tree = BP->WidgetTree;
    if (!Tree) return false;
    // 생성 후 디자이너가 바꾼 배치는 덮어쓰지 않고 연결 이름만 검사한다.
    if (Tree->RootWidget)
        return Tree->FindWidget<UTextBlock>(TEXT("Status")) && Tree->FindWidget<UTextBlock>(TEXT("Target"))
            && Tree->FindWidget<UTextBlock>(TEXT("Reason")) && Tree->FindWidget<UProgressBar>(TEXT("Progress"))
            && Tree->FindWidget<UButton>(TEXT("ReadyButton")) && Tree->FindWidget<UButton>(TEXT("StartButton"))
            && Tree->FindWidget<UButton>(TEXT("RestartButton"));
    BP->Modify();
    Tree->Modify();
    auto* Canvas = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    Tree->RootWidget = Canvas;
    auto* Panel = Tree->ConstructWidget<UBorder>();
    Panel->SetBrushColor(FLinearColor(0.015f,0.025f,0.04f,0.85f));
    Panel->SetPadding(FMargin(12));
    auto* PanelSlot = Canvas->AddChildToCanvas(Panel);
    PanelSlot->SetPosition(FVector2D(18,18));
    PanelSlot->SetAutoSize(true);
    auto* Stack = Tree->ConstructWidget<UVerticalBox>();
    Panel->SetContent(Stack);
    auto AddText = [Tree,Stack](const TCHAR* Name, int32 Size)
    {
        auto* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        auto Font = Text->GetFont(); Font.Size = Size; Text->SetFont(Font);
        Text->SetWrapTextAt(475); Stack->AddChildToVerticalBox(Text); return Text;
    };
    AddText(TEXT("Status"),16);
    AddText(TEXT("Target"),16)->SetColorAndOpacity(FLinearColor(0.15f,0.8f,1));
    auto* Progress = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Progress"));
    Progress->SetFillColorAndOpacity(FLinearColor(0.15f,0.8f,1));
    Stack->AddChildToVerticalBox(Progress);
    AddText(TEXT("Reason"),14);
    auto AddButton = [Tree,Stack](const TCHAR* Name, const TCHAR* Label)
    {
        auto* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        auto* Text = Tree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Label)); Button->SetContent(Text); Stack->AddChildToVerticalBox(Button);
    };
    AddButton(TEXT("ReadyButton"), TEXT("준비 / 준비 취소"));
    AddButton(TEXT("StartButton"), TEXT("호스트: 임무 시작"));
    AddButton(TEXT("RestartButton"), TEXT("호스트: 같은 맵 새 라운드"));
    auto* Crosshair = Tree->ConstructWidget<UTextBlock>();
    Crosshair->SetText(FText::FromString(TEXT("+")));
    auto* CrossSlot = Canvas->AddChildToCanvas(Crosshair);
    CrossSlot->SetAnchors(FAnchors(0.5f)); CrossSlot->SetAlignment(FVector2D(0.5f)); CrossSlot->SetAutoSize(true);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    return true;
#else
    return false;
#endif
}

bool USP1EditorLibrary::AddMechanicHUD(UObject* Blueprint)
{
#if WITH_EDITOR
    auto* BP = Cast<UWidgetBlueprint>(Blueprint);
    if (!BP || BP->GetPathName() != TEXT("/Game/SpacePirate/Prototype01/UI/WBP_SP1HUD.WBP_SP1HUD")) return false;
    UWidgetTree* Tree = BP->WidgetTree;
    if (!Tree || !Tree->RootWidget) return false;
    if (Tree->FindWidget(TEXT("MechanicText"))) return true;
    auto* Status = Tree->FindWidget<UTextBlock>(TEXT("Status"));
    auto* Stack = Status ? Cast<UVerticalBox>(Status->GetParent()) : nullptr;
    if (!Stack) return false;
    BP->Modify(); Tree->Modify();
    auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("MechanicText"));
    auto Font = Label->GetFont(); Font.Size = 13; Label->SetFont(Font); Label->SetWrapTextAt(475);
    Stack->AddChildToVerticalBox(Label);
    BP->OnVariableAdded(Label->GetFName());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    return true;
#else
    return false;
#endif
}

bool USP1EditorLibrary::AddSurvivalHUD(UObject* Blueprint)
{
#if WITH_EDITOR
    auto* BP = Cast<UWidgetBlueprint>(Blueprint);
    if (!BP || BP->GetPathName() != TEXT("/Game/SpacePirate/Prototype01/UI/WBP_SP1HUD.WBP_SP1HUD")) return false;
    UWidgetTree* Tree = BP->WidgetTree;
    if (!Tree || !Tree->RootWidget) return false;
    if (Tree->FindWidget(TEXT("SurvivalText"))) return Tree->FindWidget(TEXT("SurvivalCapacity")) && Tree->FindWidget(TEXT("SurvivalStamina"));
    TArray<UWidget*> Widgets; Tree->GetAllWidgets(Widgets);
    UVerticalBox* Stack = nullptr;
    for (UWidget* Widget : Widgets) if (auto* Box = Cast<UVerticalBox>(Widget)) { Stack = Box; break; }
    if (!Stack) return false;
    BP->Modify(); Tree->Modify();
    auto* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("SurvivalText"));
    auto Font = Label->GetFont(); Font.Size = 14; Label->SetFont(Font); Label->SetWrapTextAt(475);
    Stack->AddChildToVerticalBox(Label);
    auto* Size = Tree->ConstructWidget<USizeBox>(); Size->SetHeightOverride(18); Stack->AddChildToVerticalBox(Size);
    auto* Overlay = Tree->ConstructWidget<UOverlay>(); Size->SetContent(Overlay);
    auto AddBar = [Tree,Overlay](const TCHAR* Name, FLinearColor Color, bool bBase)
    {
        auto* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),Name);
        FProgressBarStyle Style = Bar->GetWidgetStyle();
        Style.BackgroundImage.TintColor = FSlateColor(FLinearColor::Transparent); Bar->SetWidgetStyle(Style);
        Bar->SetFillColorAndOpacity(Color); Bar->SetPercent(bBase ? 1.0f : 0.0f);
        auto* Slot = Overlay->AddChildToOverlay(Bar); Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
    };
    // 100칸 하나에 상처(오른쪽 빨강), 비어 있는 최대 용량, 현재 S를 차례로 겹친다.
    AddBar(TEXT("SurvivalWounds"), FLinearColor(0.8f,0.035f,0.025f), true);
    AddBar(TEXT("SurvivalCapacity"), FLinearColor(0.08f,0.10f,0.12f), false);
    AddBar(TEXT("SurvivalStamina"), FLinearColor(0.05f,0.75f,0.65f), false);
    // UE 5.8 컴파일러는 디자이너의 모든 위젯을 이름/GUID 맵에서 찾는다.
    // 기존 식별자는 유지하고 이번에 추가한 위젯만 공식 편집 API로 등록한다.
    Widgets.Reset(); Tree->GetAllWidgets(Widgets);
    for (UWidget* Widget : Widgets)
        if (!BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
            BP->OnVariableAdded(Widget->GetFName());
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    return true;
#else
    return false;
#endif
}
