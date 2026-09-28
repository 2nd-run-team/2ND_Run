// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// 역할: 운반 가능한 화물. 운반자 지정과 부착은 서버에서만 하고, 운반자는 복제되어 클라이언트 충돌을 맞춘다.
// 부착 위치 자체는 bReplicateMovement의 부착 복제로 클라이언트에 전달된다.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPCargo.generated.h"

class APawn;
class UStaticMeshComponent;

/**
 * 화물 무게 등급. 화물 BP의 Class Defaults나 배치한 인스턴스의 Details에서 고른다.
 * Small/Mid는 운반자 이동속도 배율만 다르다(배율은 SPCharacterMovementComponent에서 조절).
 */
UENUM(BlueprintType)
enum class ESPCargoWeight : uint8
{
	Small,
	Mid,
	Large // 별도 동작을 구현할 예정. 지금은 속도 변화가 없다.
};

UCLASS()
class SPACEPIRATE_API ASPCargo : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASPCargo();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnRep_AttachmentReplication() override;

	/** 서버 전용. 운반자의 HoldPoint에 붙이고 충돌을 끈다. 이미 운반 중이면 실패한다. */
	bool AttachToCarrier(APawn* Carrier, USceneComponent* HoldPoint);

	/** 서버 전용. 운반자에게서 떼어 DropLocation에 두고 물리 낙하를 시작한다. */
	bool DetachFromCarrier(const FVector& DropLocation);

	bool IsCarried() const { return IsValid(CurrentCarrier); }

	ESPCargoWeight GetWeight() const { return Weight; }

	int32 GetCargoValue() const { return CargoValue; }

	/** 개별 이동속도 배율. 화물에서 켜지 않았으면 비어 있고, 이동 컴포넌트가 무게 등급 기본값을 쓴다. */
	TOptional<float> GetCarrySpeedOverride() const
	{
		return bOverrideCarrySpeedMultiplier
			? TOptional<float>(CarrySpeedMultiplier)
			: TOptional<float>();
	}

	/** 양손 화물. 든 동안에는 다른 물건을 집거나 칸을 바꿀 수 없다. */
	bool IsTwoHanded() const { return Weight == ESPCargoWeight::Mid; }

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Cargo")
	TObjectPtr<UStaticMeshComponent> CargoMesh;

	/** HoldPoint 기준 위치. 화물 크기마다 BP에서 맞춘다. */
	UPROPERTY(EditAnywhere, Category = "Cargo")
	FVector CarryLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Cargo")
	FRotator CarryRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo")
	ESPCargoWeight Weight = ESPCargoWeight::Small;

	/**
	 * 화물 가치(판매·점수용). 화물 BP 또는 배치한 인스턴스에서 정한다.
	 * 모든 머신이 같은 BP/레벨 데이터를 읽으므로 복제하지 않는다. 스폰 시 서버에서 무작위로 정하게 되면 Replicated로 바꾼다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cargo", meta = (ClampMin = "0"))
	int32 CargoValue = 0;

	/** 켜면 무게 등급 기본 배율(캐릭터 Character Movement의 Movement|Cargo) 대신 이 화물만의 배율을 쓴다. */
	UPROPERTY(EditAnywhere, Category = "Cargo", meta = (InlineEditConditionToggle))
	bool bOverrideCarrySpeedMultiplier = false;

	/** 이 화물을 가진 동안 중력 상태 최대 속도에 곱한다. 가진 다른 화물의 배율과 누적해 곱해진다. */
	UPROPERTY(EditAnywhere, Category = "Cargo",
		meta = (EditCondition = "bOverrideCarrySpeedMultiplier", ClampMin = "0.1", ClampMax = "1.0"))
	float CarrySpeedMultiplier = 1.0f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentCarrier)
	TObjectPtr<APawn> CurrentCarrier;

	UFUNCTION()
	void OnRep_CurrentCarrier();
};
