#pragma once

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 구조용 이동 취소·시야 검사 옵션과 서버의 조작자 검증·시작 위치 저장 항목을 추가한다.

// 역할: E로 조작하는 대상(포장할 금고 묶음, 쓰러진 동료, 단말, 금고 등)에 붙인다. 기획: Docs/E_길게누르기_기획안_2026-10-05.md
// HoldDuration이 0이면 누르는 즉시 완료하고, 0보다 크면 그 시간 동안 누르고 있어야 완료한다.
// 시작·취소·완료는 서버에서만 판정하고, 조작자와 시작 시각을 복제해 모든 화면이 같은 진행률을 계산한다.
// 한 번에 한 명만 조작한다. 조작자와 대상의 거리가 시작할 때 받은 거리를 넘으면 취소한다.
// 완료 결과(가방 생성, 문 열기 등)는 OnCompleted에 기능별로 붙인다.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPStealthTypes.h"
#include "SPInteractableComponent.generated.h"

class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPInteractCompletedSignature, APawn*, User);
DECLARE_MULTICAST_DELEGATE_OneParam(FSPInteractCompletedNative, APawn*);
DECLARE_DELEGATE_RetVal_OneParam(bool, FSPCanInteractNative, APawn*);
DECLARE_DELEGATE_RetVal_OneParam(FText, FSPBlockedPromptNative, APawn*);

UCLASS(ClassGroup = (SpacePirate), meta = (BlueprintSpawnableComponent))
class SPACEPIRATE_API USPInteractableComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPInteractableComponent();

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    /** 누르는 시간(초). 0이면 짧게 누르기다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (ClampMin = "0.0"))
    float HoldDuration = 0.0f;

    /** 손을 떼거나 취소돼도 쌓인 시간을 남긴다. 금고 직접 해제처럼 누적되는 작업에 켠다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    bool bKeepProgress = false;

    /** 누르는 동안 조작자의 이동과 시점 회전을 막는다. 금고 직접 해제에만 켠다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    bool bLockControls = false;

    /** 구조처럼 제자리에서 해야 하는 작업. 시작 위치에서 움직이면 진행을 취소한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    bool bCancelOnMovement = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (ClampMin = "0.0", Units = "cm", EditCondition = "bCancelOnMovement"))
    float MovementCancelTolerance = 5.0f;

    /** 켠 대상만 시작할 때와 누르는 동안 벽에 가려졌는지 서버에서 확인한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    bool bRequireLineOfSight = false;

    /** 화면 안내에 쓸 행동 이름(예: 포장, 구조). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    FText Prompt;

    /** E를 눌렀는데 CanInteract가 거절하면 잠깐 보여 줄 안내(예: 키카드 필요). 비우면 안내하지 않는다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    FText BlockedPrompt;

    /** 남은 시간처럼 바뀌는 거절 안내가 필요한 C++ 소유자가 묶는다. 묶여 있으면 BlockedPrompt 대신 쓴다. */
    FSPBlockedPromptNative BlockedPromptNative;

    FText GetBlockedPrompt(APawn* User) const
    {
        return BlockedPromptNative.IsBound() ? BlockedPromptNative.Execute(User) : BlockedPrompt;
    }

    /** 범죄 작업만 명시적으로 지정한다. 일반 줍기·가방 운반 등에 사용하는 상호작용은 None을 유지한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interact|Crime") ESPCrimeKind CrimeKind = ESPCrimeKind::None;
    /** false: 홀드 작업 중 지속 범죄. true: 성공 완료 시점의 시야로 순간 범죄를 판정한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interact|Crime") bool bInstantCrime = false;
    UPROPERTY(BlueprintAssignable, Category="Interact") FSPInteractCompletedSignature OnStarted;
    UPROPERTY(BlueprintAssignable, Category="Interact") FSPInteractCompletedSignature OnCancelled;
    FSPInteractCompletedNative OnStartedNative;
    FSPInteractCompletedNative OnCancelledNative;

    /** 서버에서만 호출된다. 짧게 누르는 대상은 누른 즉시 호출된다. */
    UPROPERTY(BlueprintAssignable, Category = "Interact")
    FSPInteractCompletedSignature OnCompleted;

    /** OnCompleted의 C++ 버전. 같은 시점에 함께 호출된다. */
    FSPInteractCompletedNative OnCompletedNative;

    /** C++ 소유자의 조작 조건(예: 화물은 줍는 사람 인벤토리에 자리가 있어야 함). 묶여 있으면 CanInteract 기본 구현이 따른다. */
    FSPCanInteractNative CanInteractNative;

    /**
     * 기능별 조작 조건(예: 경비 뒤에서만 소매치기). 시작할 때와 누르는 동안 서버가 계속 확인하고,
     * 클라이언트도 요청을 보내기 전에 미리 확인한다. BP에서 덮어쓸 수 있다.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Interact")
    bool CanInteract(APawn* User) const;

    /**
     * 서버 전용. 짧게 누르는 대상이면 바로 완료하고 false를 돌려준다.
     * 길게 누르는 대상이면 누르기를 시작하고 true를 돌려준다. 조건이 맞지 않으면 false다.
     */
    bool TryInteract(APawn* User, float MaxDistance);

    /** 서버 전용. User가 조작 중이면 취소한다. */
    void CancelBy(const APawn* User);

    /** 0～1. 진행률 보존 대상은 아무도 누르지 않아도 쌓인 만큼을 돌려준다. */
    UFUNCTION(BlueprintPure, Category = "Interact")
    float GetProgress() const;

    UFUNCTION(BlueprintPure, Category = "Interact")
    APawn* GetCurrentUser() const { return CurrentUser; }

    bool IsInstant() const { return HoldDuration <= 0.0f; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
    void Finish(bool bCompleted);
    double GetSyncedTime() const;
    bool IsUserAvailable(const APawn* User) const;
    bool HasLineOfSight(const APawn* User) const;

    UPROPERTY(Replicated)
    TObjectPtr<APawn> CurrentUser;

    /** 서버 기준 시작 시각. 클라이언트가 진행률을 계산하는 데만 쓴다. */
    UPROPERTY(Replicated)
    double StartTime = 0.0;

    /** 진행률 보존 대상의 지난 조작에서 쌓인 시간(초). */
    UPROPERTY(Replicated)
    float SavedSeconds = 0.0f;

    /** 서버 전용. 이번 조작에서 누른 시간(초). 완료 판정은 이 값으로 한다. */
    float HeldSeconds = 0.0f;

    /** 서버 전용. 시작할 때 받은 상호작용 거리. */
    float AllowedDistance = 0.0f;
    FGuid CrimeRegistration;
    bool bTransitioning = false;
    bool bStartedWithController = false;

    /** 서버 전용. 구조자가 시작한 위치. */
    FVector UserStartLocation = FVector::ZeroVector;
};
