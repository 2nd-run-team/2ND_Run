#pragma once

// 역할: 1차 MVP안 06장의 인벤토리(기본 4칸, SlotCount로 조절). 슬롯과 현재 칸을 복제하고, 집기/버리기/전환은 서버에서만 판정한다.
// 물건은 종류마다 한 칸을 쓰고 쌓지 않는다. 등 가방도 한 칸을 쓰며 한 사람이 하나만 멘다.
// 손에 보이는 것은 현재 칸의 물건뿐이다. 나머지 칸의 물건은 숨긴 채 같은 손 지점에 붙여 두고, 가방은 항상 등에 보인다.
// 가방 칸을 고르면 손은 비어 있다. 버리기는 현재 칸의 물건에 적용하고, 던지기는 가방만 한다.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPCargo.h"
#include "SPInventoryComponent.generated.h"

class ASPCargo;
class USceneComponent;

UCLASS(ClassGroup = (SpacePirate), meta = (BlueprintSpawnableComponent))
class SPACEPIRATE_API USPInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USPInventoryComponent();

    virtual void PostInitProperties() override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 물건이 붙을 위치. 소유 캐릭터가 생성자에서 지정한다. */
    void SetAttachPoints(USceneComponent* InHandPoint, USceneComponent* InBackPoint)
    {
        HandPoint = InHandPoint;
        BackPoint = InBackPoint;
    }

    /** 현재 칸의 물건. 가방일 수도 있다. 비어 있으면 null. */
    ASPCargo* GetActiveItem() const;

    /** 손에 든 물건. 현재 칸이 비었거나 가방이면 null. */
    ASPCargo* GetHandItem() const;

    /** 등에 멘 가방이 있는지. 이동속도와 달리기 제한에 쓴다. */
    bool HasBag() const;

    const TArray<TObjectPtr<ASPCargo>>& GetSlots() const { return Slots; }

    /** 클라이언트의 사전 확인과 서버 판정이 같은 규칙을 쓴다. */
    bool CanPickUp(const ASPCargo* Item) const;

    /** 서버 전용. 화물의 상호작용(짧게 누르기)이 완료되면 부른다. 거리는 상호작용에서 이미 확인했다. */
    void PickUp(ASPCargo* Item);

    /** 그 종류의 물건 중 첫 번째. 없으면 null. 키카드 문, 드릴 설치 조건처럼 "가졌는가"를 볼 때 쓴다. */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    ASPCargo* FindItemOfType(ESPItemType Type) const;

    /** 서버 전용. 물건을 칸에서 빼고 없앤다. 드릴 적재·설치처럼 물건이 다른 것으로 바뀌는 완료 처리에 쓴다(MVP안 06장). */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
    void ConsumeItem(ASPCargo* Item);

    /** 서버 전용. 등 가방만 그 자리에 떨어뜨리고 나머지 칸은 유지한다. 다운될 때 쓴다(MVP안 06장). */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
    void DropBag();

    /** 현재 칸의 물건을 앞에 내려놓는다. 칸 번호는 유지한다. */
    UFUNCTION(Server, Reliable)
    void ServerDrop();

    /** 현재 칸의 가방을 시선 방향으로 던진다. Charge(0～1)로 속도를 정한다. 가방이 아니면 내려놓는다. */
    UFUNCTION(Server, Reliable)
    void ServerThrow(float Charge);

    UFUNCTION(Server, Reliable)
    void ServerSelectSlot(int32 SlotIndex);

    /** 휠 입력. 연속 입력이 복제 지연에 묻히지 않도록 서버의 현재 칸 기준으로 이동한다. */
    UFUNCTION(Server, Reliable)
    void ServerCycleSlot(int32 Direction);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 인벤토리 칸 수. 바꾸면 IMC의 숫자키 매핑도 칸 수에 맞춰야 한다. */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "1", ClampMax = "9"))
    int32 SlotCount = 4;

    /** 버릴 자리를 찾는 트레이스 채널. 벽·바닥·다른 플레이어가 이 채널을 Block해야 물건이 겹쳐 놓이지 않는다. */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory", AdvancedDisplay)
    TEnumAsByte<ECollisionChannel> DropTraceChannel = ECC_WorldDynamic;

    /** 버릴 때 소유자 충돌 표면과 물건 사이에 두는 여유 거리. 물리가 켜진 직후 캡슐에 튕기지 않게 한다. */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory", AdvancedDisplay, meta = (ClampMin = "0.0"))
    float DropGap = 10.0f;

    // 던지기 속도는 MVP안에 없는 시험값이다. 플레이 테스트로 정한다.
    /** 차징 없이 던질 때의 속도(cm/s). */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Throw", meta = (ClampMin = "0.0"))
    float MinThrowSpeed = 400.0f;

    /** 끝까지 차징했을 때의 속도(cm/s). */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory|Throw", meta = (ClampMin = "0.0"))
    float MaxThrowSpeed = 1000.0f;

private:
    int32 FindSlotForPickUp() const;
    void ApplyActiveSlot(int32 NewSlot);
    void DropActiveItem(const FVector& Velocity);
    bool FindDropLocation(const ASPCargo& Item, FVector& OutLocation) const;
    void DropAll();

    UFUNCTION()
    void OnRep_Inventory();

    UPROPERTY(ReplicatedUsing = OnRep_Inventory)
    TArray<TObjectPtr<ASPCargo>> Slots;

    UPROPERTY(ReplicatedUsing = OnRep_Inventory)
    int32 ActiveSlot = 0;

    UPROPERTY()
    TObjectPtr<USceneComponent> HandPoint;

    UPROPERTY()
    TObjectPtr<USceneComponent> BackPoint;
};
