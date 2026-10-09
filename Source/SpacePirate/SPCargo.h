#pragma once

// 역할: 인벤토리에 넣는 물건(키카드, 제압 도구, 소형 전리품, 드릴 가방, 전리품 가방). 1차 MVP안 06장.
// 운반자 지정과 부착은 서버에서만 하고, 운반자는 복제되어 클라이언트 충돌을 맞춘다.
// 부착 위치 자체는 bReplicateMovement의 부착 복제로 클라이언트에 전달된다.
// 가방은 손이 아니라 등에 붙는다. 칸은 다른 물건처럼 하나 차지한다.
// 줍기는 짧게 누르는 상호작용(Interactable, 시간 0)이다. 완료되면 서버가 줍는 사람의 인벤토리에 넣는다.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPCargo.generated.h"

class APawn;
class UStaticMeshComponent;
class USPInteractableComponent;

/** 물건 종류. 물건 BP의 Class Defaults나 배치한 인스턴스의 Details에서 고른다. */
UENUM(BlueprintType)
enum class ESPItemType : uint8
{
	Keycard,
	CCTool,   // CC(군중 제어) 도구. MVP안 05장의 제압 도구
	SmallLoot,
	DrillBag, // 등 가방
	LootBag   // 등 가방
};

UCLASS()
class SPACEPIRATE_API ASPCargo : public AActor
{
	GENERATED_BODY()

public:
	ASPCargo();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnRep_AttachmentReplication() override;

	/** 서버 전용. 운반자의 AttachPoint(손 또는 등)에 붙이고 충돌을 끈다. 이미 운반 중이면 실패한다. */
	bool AttachToCarrier(APawn* Carrier, USceneComponent* AttachPoint);

	/** 서버 전용. 운반자에게서 떼어 DropLocation에 두고 Velocity로 물리 낙하를 시작한다. */
	bool DetachFromCarrier(const FVector& DropLocation, const FVector& Velocity = FVector::ZeroVector);

	bool IsCarried() const { return IsValid(CurrentCarrier); }

	ESPItemType GetItemType() const { return ItemType; }

	/** 등에 메는 가방. 한 사람이 하나만 멘다. */
	bool IsBag() const { return ItemType == ESPItemType::DrillBag || ItemType == ESPItemType::LootBag; }

	int32 GetCargoValue() const { return CargoValue; }

	USPInteractableComponent* GetInteractable() const { return Interactable; }

	/**
	 * 서버 전용. 보관함 안에 놓인 물건으로 표시한다(ASPOpenable이 시작할 때 부른다).
	 * 처음 줍는 순간만 범죄(ContainerTheft)로 보고, 주우면 표시를 지운다. 내려놓았다 다시 줍는 것은 일반 줍기다.
	 */
	void MarkStoredInContainer();

protected:
	/** 이름은 기존 BP의 메시 설정을 유지하려고 바꾸지 않는다. */
	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UStaticMeshComponent> CargoMesh;

	/** E 줍기. 시간 0(짧게 누르기)으로 두고 바꾸지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USPInteractableComponent> Interactable;

	virtual void BeginPlay() override;

	/** 붙는 지점(손 또는 등) 기준 위치. 물건마다 BP에서 맞춘다. */
	UPROPERTY(EditAnywhere, Category = "Item")
	FVector CarryLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Item")
	FRotator CarryRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	ESPItemType ItemType = ESPItemType::Keycard;

	/**
	 * 전리품 가치(정산용). 도구와 키카드는 0으로 둔다. 물건 BP 또는 배치한 인스턴스에서 정한다.
	 * 모든 머신이 같은 BP/레벨 데이터를 읽으므로 복제하지 않는다. 스폰 시 서버에서 정하게 되면 Replicated로 바꾼다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "0"))
	int32 CargoValue = 0;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentCarrier)
	TObjectPtr<APawn> CurrentCarrier;

	UFUNCTION()
	void OnRep_CurrentCarrier();

	void StartPhysicsFromServerState();

	bool CanBePickedUpBy(APawn* User) const;
	void HandlePickedUp(APawn* User);
};
