// 작성자 : 임진혁 (P2 사망 시 서버 소지품 해제 진입점)
#pragma once

// 역할: 리썰 컴퍼니식 인벤토리(기본 3칸, SlotCount로 조절). 슬롯과 현재 칸을 복제하고, 집기/버리기/전환은 서버에서만 판정한다.
// 손에 보이는 것은 현재 칸의 화물뿐이며, 나머지 칸의 화물은 숨긴 채 같은 HoldPoint에 붙여 둔다.
// 양손(Mid) 화물을 든 동안에는 집기와 칸 전환이 막히고, 버려야 풀린다.
// Large는 칸에 넣지 않고 따로 잡는다. 현재 칸이 비어 있어야 잡을 수 있고, 잡는 동안 집기와 칸 전환이 막히며, 버리기 키로 놓는다.

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

    /** 화물이 붙을 위치. 소유 캐릭터가 생성자에서 지정한다. */
    void SetHoldPoint(USceneComponent* InHoldPoint) { HoldPoint = InHoldPoint; }

    /** 현재 칸의 화물. 비어 있으면 null. */
    ASPCargo* GetActiveCargo() const;

    const TArray<TObjectPtr<ASPCargo>>& GetSlots() const { return Slots; }

    /** 잡고 있는 Large. 없으면 null. */
    ASPCargo* GetGrippedLarge() const;

    bool IsGrippingLarge() const { return GetGrippedLarge() != nullptr; }

    /** 서버 전용. Large가 이 소유자를 잡는 지점에서 뺄 때 부른다. */
    void OnLargeReleased(const ASPCargo* Cargo);
    /** 서버 사망 전이 전용. 숨긴 슬롯까지 해제하고, 빈 인벤토리에 재호출해도 화물을 다시 떨어뜨리지 않는다. */
    void ReleaseAllForDeath();

    /** 클라이언트의 사전 확인과 서버 판정이 같은 규칙을 쓴다. */
    bool CanPickUp(const ASPCargo* Cargo) const;

    UFUNCTION(Server, Reliable)
    void ServerPickUp(ASPCargo* Cargo);

    /** 현재 칸의 화물을 앞에 떨어뜨린다. 칸 번호는 유지한다. Large를 잡고 있으면 그것을 놓는다. */
    UFUNCTION(Server, Reliable)
    void ServerDrop();

    UFUNCTION(Server, Reliable)
    void ServerSelectSlot(int32 SlotIndex);

    /** 휠 입력. 연속 입력이 복제 지연에 묻히지 않도록 서버의 현재 칸 기준으로 이동한다. */
    UFUNCTION(Server, Reliable)
    void ServerCycleSlot(int32 Direction);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 인벤토리 칸 수. 바꾸면 IMC의 숫자키 매핑도 칸 수에 맞춰야 한다. */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "1", ClampMax = "9"))
    int32 SlotCount = 3;

    /** 버릴 자리를 찾는 트레이스 채널. 벽·바닥·다른 플레이어가 이 채널을 Block해야 화물이 겹쳐 놓이지 않는다. */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory", AdvancedDisplay)
    TEnumAsByte<ECollisionChannel> DropTraceChannel = ECC_WorldDynamic;

    /** 서버가 허용하는 소유자-화물 최대 거리. 지연과 액터 원점 차이를 감안해 클라이언트 트레이스보다 크게 둔다. */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = "0.0"))
    float ServerPickupRange = 400.0f;

    /** 버릴 때 소유자 충돌 표면과 화물 사이에 두는 여유 거리. 물리가 켜진 직후 캡슐에 튕기지 않게 한다. */
    UPROPERTY(EditDefaultsOnly, Category = "Inventory", AdvancedDisplay, meta = (ClampMin = "0.0"))
    float DropGap = 10.0f;

private:
    bool IsHoldingTwoHanded() const;
    int32 FindSlotForPickUp() const;
    void ApplyActiveSlot(int32 NewSlot);
    bool FindDropLocation(const ASPCargo& Cargo, FVector& OutLocation) const;
    void DropAll();

    UFUNCTION()
    void OnRep_Inventory();

    UPROPERTY(ReplicatedUsing = OnRep_Inventory)
    TArray<TObjectPtr<ASPCargo>> Slots;

    UPROPERTY(ReplicatedUsing = OnRep_Inventory)
    int32 ActiveSlot = 0;

    /** 이동속도 예측과 클라이언트 사전 확인에 쓰므로 복제한다. */
    UPROPERTY(ReplicatedUsing = OnRep_Inventory)
    TObjectPtr<ASPCargo> GrippedLarge;

    UPROPERTY()
    TObjectPtr<USceneComponent> HoldPoint;
};
