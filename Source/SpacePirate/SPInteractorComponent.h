#pragma once

// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 상호작용 시작·진행 시 다운 여부를 확인하는 CanOwnerInteract 선언을 추가한다.

// 역할: 플레이어의 E 상호작용 전부(화물 줍기 포함)를 맡는다. 화면 중앙의 USPInteractableComponent 대상을 찾아
// 누르고 떼는 요청을 서버로 보내고, 누르는 중인지와 진행률을 알려 준다.
// 누르는 동안의 입력 무시(이동·시점 외)와 조작 잠금은 캐릭터가 IsHolding / IsControlLocked로 확인한다.
// 로컬 플레이어 화면에 진행 바 위젯을 띄우고 숨긴다. 위젯이 지정되지 않으면 디버그 텍스트로 대신한다.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPInteractorComponent.generated.h"

class APawn;
class UUserWidget;
class USPInteractableComponent;

UCLASS(ClassGroup = (SpacePirate), meta = (BlueprintSpawnableComponent))
class SPACEPIRATE_API USPInteractorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPInteractorComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    /** 클라이언트에서 E를 눌렀을 때 부른다. 화면 중앙 대상을 찾아 StartInteract로 넘긴다. */
    void Press();

    /** 대상을 직접 지정해 누른다. 짧게 누르는 대상이면 서버가 바로 완료한다. */
    void StartInteract(USPInteractableComponent* Target);

    /** 클라이언트에서 E를 뗐을 때 부른다. */
    UFUNCTION()
    void StopInteract();

    /** 서버가 이 플레이어의 누르기를 받아들여 진행 중인지. 서버와 클라이언트 모두에서 쓸 수 있다. */
    bool IsHolding() const;

    /** 누르는 대상이 이동과 시점 회전을 막는지(금고 직접 해제). */
    bool IsControlLocked() const;

    /** 0～1. 누르는 중이 아니면 0. 진행 바 위젯이 읽는다. */
    UFUNCTION(BlueprintPure, Category = "Interact")
    float GetHoldProgress() const;

    /** 누르는 대상의 행동 이름(예: 포장). 누르는 중이 아니면 비어 있다. */
    UFUNCTION(BlueprintPure, Category = "Interact")
    FText GetHoldPrompt() const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 서버가 허용하는 플레이어-대상 거리. 화면의 상호작용 거리(TraceDistance)에 지연 여유 50cm를 더한 시험값이다. */
    UPROPERTY(EditDefaultsOnly, Category = "Interact", meta = (ClampMin = "0.0"))
    float ServerInteractRange = 300.0f;

    /** 화면 중앙에서 대상을 찾는 구체 트레이스 길이(상호작용 거리). */
    UPROPERTY(EditDefaultsOnly, Category = "Interact", meta = (ClampMin = "0.0"))
    float TraceDistance = 250.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Interact", meta = (ClampMin = "0.0"))
    float TraceRadius = 20.0f;

    /** 대상을 찾는 트레이스 채널. 대상 메시가 이 채널을 Block해야 조작할 수 있다. */
    UPROPERTY(EditDefaultsOnly, Category = "Interact", AdvancedDisplay)
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** 누르는 동안 화면 가운데에 띄울 위젯(WBP_SPHoldProgress). 비우면 디버그 텍스트로 대신한다. */
    UPROPERTY(EditDefaultsOnly, Category = "Interact")
    TSubclassOf<UUserWidget> HoldProgressWidgetClass;

private:
    UFUNCTION(Server, Reliable)
    void ServerStartInteract(AActor* TargetActor);

    UFUNCTION(Server, Reliable)
    void ServerStopInteract();

    void UpdateHoldDisplay();
    USPInteractableComponent* FindTargetInView() const;
    bool CanOwnerInteract() const;

    /** 클라이언트는 누른 대상을, 서버는 받아들인 대상을 기억한다. 진행 여부는 대상의 조작자로 확인한다. */
    UPROPERTY(Transient)
    TObjectPtr<USPInteractableComponent> HoldTarget;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> HoldWidget;
};
