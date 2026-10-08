#pragma once

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 신규 작성
// 변경 내용: Widget BP로 외형을 교체할 수 있는 임시 상태 HUD와 표시 항목을 정의한다.
// 작업자: 김세훈 | 2026-10-08 | 디버그 도움말 분리
// 변경 내용: 기본 HUD의 조작 안내를 없애고 기존 BP의 HintText·함수 참조만 호환용으로 보존한다.
// 작업자: 김세훈 | 2026-10-08 | 도움말 키 안내
// 변경 내용: 체력 패널 위에 옅게 표시할 도움말 열기 안내 TextBlock을 추가한다.

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SPPlayerStatusHUDWidget.generated.h"

class UTextBlock;
class USPPlayerStatusComponent;

/** 임시 숫자 체력 HUD. 기본 UI를 코드로 생성하며 Widget BP로 외형을 교체할 수도 있다. */
UCLASS(Blueprintable)
class SPACEPIRATE_API USPPlayerStatusHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** 기존 BP 호환용. 디버그 조작법은 이제 SPDebugHelpWidget에 표시한다. */
    UFUNCTION(BlueprintCallable, Category = "Status|HUD", meta = (DeprecatedFunction,
        DeprecationMessage = "Debug controls are now shown in SPDebugHelpWidget."))
    void SetShowDebugControls(bool bShow) {}

    /** BP 위젯에서도 같은 복제 상태를 읽을 수 있다. HUD 자체에는 게임 상태를 저장하지 않는다. */
    UFUNCTION(BlueprintPure, Category = "Status|HUD")
    USPPlayerStatusComponent* GetDisplayedStatusComponent() const;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // BP Designer에 같은 이름의 TextBlock을 두면 갱신 코드를 그대로 활용할 수 있다.
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Status|HUD")
    TObjectPtr<UTextBlock> HealthText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Status|HUD")
    TObjectPtr<UTextBlock> StateText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Status|HUD")
    TObjectPtr<UTextBlock> RescueText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Status|HUD")
    TObjectPtr<UTextBlock> TeamText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Status|HUD")
    TObjectPtr<UTextBlock> HelpHintText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Status|HUD")
    TObjectPtr<UTextBlock> HintText;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Status|HUD")
    TObjectPtr<UTextBlock> FailureText;

private:
    void BuildDefaultWidgetTree();
    void RefreshDisplay();

};
