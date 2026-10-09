#pragma once

// 역할: E로 열면 문짝이 비켜나고 계속 열려 있는 대상. 키카드 보안문, 일반 보관함, 소형 금고, 화물 상자, 특수 금고의 문.
// 기획: Docs/인벤토리_아이템_구현안_2026-10-08.md 2.3장. 필요 물건, 여는 시간, 범죄 종류, 안내 문구의 차이는 BP 설정으로 둔다.
// 닫힌 문짝이 E 트레이스(Visibility)를 막으므로 안에 둔 물건과 묶음은 열기 전에 조작할 수 없다.
// 여는 것의 범죄 판정은 Interactable의 CrimeKind/bInstantCrime으로 경비 쪽에 연결된다.
// NOTICE [INDIRECT-CLUE]: 경비의 간접 단서 검사기가 생기면 IsClue()가 참인 대상을 열린 용기 단서로 읽게 한다.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPCargo.h"
#include "SPOpenable.generated.h"

class APawn;
class UBoxComponent;
class UStaticMeshComponent;
class USPInteractableComponent;

UCLASS()
class SPACEPIRATE_API ASPOpenable : public AActor
{
    GENERATED_BODY()

public:
    ASPOpenable();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PostInitializeComponents() override;

    UFUNCTION(BlueprintPure, Category = "Openable")
    bool IsOpen() const { return bOpen; }

    /** 열려 있고, 열린 상태가 간접 단서인 대상(보관함, 금고, 상자). 경비의 단서 검사기가 읽는다. */
    UFUNCTION(BlueprintPure, Category = "Openable")
    bool IsClue() const { return bOpen && bIsClueWhenOpen; }

    USPInteractableComponent* GetInteractable() const { return Interactable; }

protected:
    virtual void BeginPlay() override;

    /** 여는 조건. 특수 금고처럼 조건이 더 있는 대상은 덮어쓰고 부모 결과와 함께 쓴다. */
    virtual bool CanOpen(APawn* User) const;

    /** 서버 전용. 열고 모든 화면에 복제한다. */
    void Open();

    UPROPERTY(VisibleAnywhere, Category = "Openable")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    /** 열리면 OpenOffset만큼 옮긴다. 닫혀 있는 동안 Visibility를 막아 안쪽 물건을 조작하지 못하게 한다. */
    UPROPERTY(VisibleAnywhere, Category = "Openable")
    TObjectPtr<UStaticMeshComponent> DoorMesh;

    /** 이 안에 놓인 물건을 시작할 때 보관함 물건(처음 줍기 = 범죄)으로 표시한다. 크기가 0이면 표시하지 않는다. */
    UPROPERTY(VisibleAnywhere, Category = "Openable")
    TObjectPtr<UBoxComponent> ContentsBox;

    /** E로 열기. 시간, 진행률 보존, 조작 잠금, 안내 문구, 범죄 종류는 BP에서 정한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Openable")
    TObjectPtr<USPInteractableComponent> Interactable;

    /** 켜면 RequiredHandItem을 손에 들어야 연다(키카드 보안문). */
    UPROPERTY(EditAnywhere, Category = "Openable")
    bool bRequiresHandItem = false;

    UPROPERTY(EditAnywhere, Category = "Openable", meta = (EditCondition = "bRequiresHandItem"))
    ESPItemType RequiredHandItem = ESPItemType::Keycard;

    /** 열렸을 때 문짝이 닫힌 자리에서 옮겨 갈 거리(문짝의 부모 기준). 문 크기에 맞게 BP에서 정한다. */
    UPROPERTY(EditAnywhere, Category = "Openable")
    FVector OpenOffset = FVector(0.0, 100.0, 0.0);

    /** 열린 상태를 경비가 보면 간접 단서인지. 보관함, 금고, 상자는 켜고 문은 끈다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Openable")
    bool bIsClueWhenOpen = false;

private:
    UPROPERTY(ReplicatedUsing = OnRep_Open)
    bool bOpen = false;

    UFUNCTION()
    void OnRep_Open();

    void HandleOpened(APawn* User);
    void MarkContents();

    /** BP에서 정한 문짝 위치. 생성자에서는 보이지 않으므로 PostInitializeComponents에서 기억한다. */
    FVector ClosedDoorLocation = FVector::ZeroVector;
};
