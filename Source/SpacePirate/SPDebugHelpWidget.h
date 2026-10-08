#pragma once

// 작업자: 김세훈 | 2026-10-08 | 프로토타입 디버그 도움말 신규 작성
// 변경 내용: 상태 HUD와 독립된 도움말 위젯과 BP에서 편집 가능한 조작 목록·페이지 기능을 정의한다.
// 작업자: 김세훈 | 2026-10-08 | 도움말 페이지 설정 변경
// 변경 내용: 페이지당 표시 개수를 10개로 늘리고 쉼표·마침표 키 사용을 명시한다.

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "SPDebugHelpWidget.generated.h"

class UTextBlock;
class UVerticalBox;

/** 안내 항목만 정의한다. 실제 키 입력과 실행 기능은 각 기능의 코드에서 연결한다. */
USTRUCT(BlueprintType)
struct FSPDebugHelpEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Help")
    FText Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Help")
    FText Keys;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Help")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Help")
    bool bHostOnly = false;

    /** 체력 시험 키가 꺼져 있으면 이 항목도 숨긴다. 다른 기능의 안내는 영향을 받지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug Help")
    bool bRequiresStatusDebugControls = false;
};

/** 게임 입력·마우스 포커스를 바꾸지 않는 로컬 도움말. H 토글, 쉼표·마침표 키로 페이지 이동. */
UCLASS(Blueprintable)
class SPACEPIRATE_API USPDebugHelpWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USPDebugHelpWidget(const FObjectInitializer& ObjectInitializer);

    void Configure(FKey InToggleKey, bool bInStatusControlsEnabled);

    UFUNCTION(BlueprintCallable, Category = "Debug Help")
    void ChangePage(int32 Direction);

    UFUNCTION(BlueprintPure, Category = "Debug Help")
    int32 GetPageCount() const;

    UFUNCTION(BlueprintPure, Category = "Debug Help")
    int32 GetCurrentPage() const { return PageIndex + 1; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

    /** 이 위젯을 상속한 BP의 Class Defaults에서 추가/수정한다. 입력 기능 자체를 만들지는 않는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debug Help")
    TArray<FSPDebugHelpEntry> HelpEntries;

    // BP로 외형을 교체할 때 같은 이름의 VerticalBox/TextBlock을 두면 목록 갱신을 재사용한다.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Debug Help")
    TObjectPtr<UVerticalBox> HelpRows;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Debug Help")
    TObjectPtr<UTextBlock> PageText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Debug Help")
    TObjectPtr<UTextBlock> CloseText;

private:
    void BuildDefaultWidgetTree();
    void RefreshPage();
    bool ShouldShowEntry(const FSPDebugHelpEntry& Entry) const;

    static constexpr int32 EntriesPerPage = 10;
    FKey ToggleKey = EKeys::H;
    bool bStatusControlsEnabled = false;
    int32 PageIndex = 0;
};
