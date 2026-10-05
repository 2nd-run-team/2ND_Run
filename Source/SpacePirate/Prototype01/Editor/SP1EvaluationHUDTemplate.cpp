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
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
namespace
{
    void RefreshWidgetIds(UWidgetBlueprint* BP)
    {
        TArray<UWidget*> Widgets; BP->WidgetTree->GetAllWidgets(Widgets);
        TSet<FName> Present;
        for (const auto* Widget : Widgets) Present.Add(Widget->GetFName());
        TArray<FName> Previous; BP->WidgetVariableNameToGuidMap.GetKeys(Previous);
        for (FName Name : Previous) if (!Present.Contains(Name)) BP->OnVariableRemoved(Name);
        for (FName Name : Present) if (!BP->WidgetVariableNameToGuidMap.Contains(Name)) BP->OnVariableAdded(Name);
    }
}
#endif

bool USP1EditorLibrary::BuildEvaluationHUD(UObject* Blueprint,bool bRebuild)
{
#if WITH_EDITOR
    auto* BP=Cast<UWidgetBlueprint>(Blueprint);
    if (!BP || BP->GetPathName()!=TEXT("/Game/SpacePirate/Prototype01/UI/WBP_SP1HUD.WBP_SP1HUD")) return false;
    if (BP->WidgetTree && BP->WidgetTree->FindWidget(TEXT("MenuPanel")) && !bRebuild)
    { BP->Modify(); RefreshWidgetIds(BP); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); return true; }
    BP->Modify();
    // P4 템플릿 교체는 이 전용 WBP에서만 명시적으로 수행한다. 이후 Designer 편집은 재실행으로 덮지 않는다.
    if (BP->WidgetTree) BP->WidgetTree->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional);
    UWidgetTree* Tree=NewObject<UWidgetTree>(BP,TEXT("WidgetTree"),RF_Transactional); BP->WidgetTree=Tree;
    auto* Canvas=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("Root")); Tree->RootWidget=Canvas;
    auto Panel=[&](const TCHAR* Name,FVector2D Anchor,FVector2D Position,FVector2D Align,float Width)
    {
        auto* Border=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);
        Border->SetBrushColor(FLinearColor(.014f,.025f,.04f,.94f)); Border->SetPadding(FMargin(18));
        auto* Slot=Canvas->AddChildToCanvas(Border); Slot->SetAnchors(FAnchors(Anchor.X,Anchor.Y));
        Slot->SetPosition(Position); Slot->SetAlignment(Align); Slot->SetAutoSize(true);
        auto* Size=Tree->ConstructWidget<USizeBox>(); Size->SetWidthOverride(Width); Border->SetContent(Size);
        auto* Box=Tree->ConstructWidget<UVerticalBox>(); Size->SetContent(Box); return Box;
    };
    auto Text=[&](UVerticalBox* Box,const TCHAR* Name,int32 Size,float Width,FLinearColor Color=FLinearColor::White)
    {
        auto* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        auto Font=T->GetFont(); Font.Size=Size; T->SetFont(Font); T->SetWrapTextAt(Width);
        T->SetColorAndOpacity(Color); auto* Slot=Box->AddChildToVerticalBox(T); Slot->SetPadding(FMargin(0,3)); return T;
    };
    auto Button=[&](UVerticalBox* Box,const TCHAR* Name,const TCHAR* Label)
    {
        auto* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
        auto* T=Tree->ConstructWidget<UTextBlock>(); auto Font=T->GetFont(); Font.Size=21; T->SetFont(Font);
        T->SetText(FText::FromString(Label)); T->SetColorAndOpacity(FLinearColor(.04f,.07f,.1f));
        B->SetContent(T); auto* Slot=Box->AddChildToVerticalBox(B); Slot->SetPadding(FMargin(0,7));
        B->SetBackgroundColor(FLinearColor(.3f,.88f,.98f)); return B;
    };
    auto* Top=Panel(TEXT("MissionPanel"),{0,0},{26,22},{0,0},620);
    Text(Top,TEXT("Status"),25,620);
    auto* Team=Panel(TEXT("TeamPanel"),{0,0},{26,146},{0,0},290);
    Text(Team,TEXT("TeamText"),18,290); Text(Team,TEXT("PingText"),16,290,FLinearColor(.55f,.88f,1));
    auto* Mechanic=Panel(TEXT("DevicePanel"),{1,0},{-26,22},{1,0},360);
    Text(Mechanic,TEXT("MechanicText"),19,360);
    auto* Life=Panel(TEXT("LifePanel"),{0,1},{26,-24},{0,1},420);
    Text(Life,TEXT("SurvivalText"),18,420);
    auto* BarSize=Tree->ConstructWidget<USizeBox>(); BarSize->SetHeightOverride(20); Life->AddChildToVerticalBox(BarSize);
    auto* Bars=Tree->ConstructWidget<UOverlay>(); BarSize->SetContent(Bars);
    const FName BarNames[]={TEXT("SurvivalWounds"),TEXT("SurvivalCapacity"),TEXT("SurvivalStamina")};
    const FLinearColor BarColors[]={FLinearColor(.8f,.055f,.04f),FLinearColor(.08f,.1f,.12f),FLinearColor(.06f,.8f,.7f)};
    for (int32 Index=0;Index<3;++Index)
    {
        auto* Bar=Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),BarNames[Index]);
        FProgressBarStyle Style=Bar->GetWidgetStyle(); Style.BackgroundImage.TintColor=FSlateColor(FLinearColor::Transparent); Bar->SetWidgetStyle(Style);
        Bar->SetFillColorAndOpacity(BarColors[Index]); Bar->SetPercent(Index==0 ? 1 : 0);
        auto* Slot=Bars->AddChildToOverlay(Bar); Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill);
    }
    auto* Target=Panel(TEXT("TargetPanel"),{.5f,1},{0,-130},{.5f,1},660);
    auto* IconSize=Tree->ConstructWidget<USizeBox>(); IconSize->SetWidthOverride(44); IconSize->SetHeightOverride(44);
    auto* IconSlot=Target->AddChildToVerticalBox(IconSize); IconSlot->SetHorizontalAlignment(HAlign_Left);
    auto* Icon=Tree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("TargetIcon")); IconSize->SetContent(Icon);
    Text(Target,TEXT("Target"),22,660,FLinearColor(.3f,.9f,1));
    auto* Progress=Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("Progress")); Progress->SetFillColorAndOpacity(FLinearColor(.2f,.85f,1));
    auto* ProgressSize=Tree->ConstructWidget<USizeBox>(); ProgressSize->SetHeightOverride(15); ProgressSize->SetContent(Progress); Target->AddChildToVerticalBox(ProgressSize);
    Text(Target,TEXT("Reason"),18,660,FLinearColor(1,.78f,.38f));
    auto* Alert=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("AlertText"));
    auto AF=Alert->GetFont(); AF.Size=26; Alert->SetFont(AF); Alert->SetWrapTextAt(790); Alert->SetJustification(ETextJustify::Center); Alert->SetColorAndOpacity(FLinearColor(1,.72f,.3f));
    auto* AS=Canvas->AddChildToCanvas(Alert); AS->SetAnchors(FAnchors(.5f,0)); AS->SetAlignment({.5f,0}); AS->SetPosition({0,145}); AS->SetAutoSize(true);
    auto* Menu=Panel(TEXT("MenuPanel"),{.5f,.5f},{0,0},{.5f,.5f},650);
    Text(Menu,TEXT("MenuTitle"),32,650,FLinearColor(.35f,.92f,1)); Text(Menu,TEXT("MenuBody"),22,650);
    Button(Menu,TEXT("ReadyButton"),TEXT("준비 / 준비 취소")); Button(Menu,TEXT("StartButton"),TEXT("호스트: 임무 시작")); Button(Menu,TEXT("RestartButton"),TEXT("같은 맵 · 새 라운드"));
    auto* Options=Panel(TEXT("AudioPanel"),{1,1},{-26,-24},{1,1},250);
    Button(Options,TEXT("MuteButton"),TEXT("피드백음 ON · 끄기"));
    auto* Cross=Tree->ConstructWidget<UTextBlock>(); Cross->SetText(FText::FromString(TEXT("+"))); auto CF=Cross->GetFont(); CF.Size=25; Cross->SetFont(CF);
    auto* CS=Canvas->AddChildToCanvas(Cross); CS->SetAnchors(FAnchors(.5f)); CS->SetAlignment({.5f,.5f}); CS->SetAutoSize(true);
    RefreshWidgetIds(BP);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    return true;
#else
    return false;
#endif
}
