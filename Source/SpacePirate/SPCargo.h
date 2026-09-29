// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// 역할: 운반 가능한 화물. 운반자 지정과 부착은 서버에서만 하고, 운반자는 복제되어 클라이언트 충돌을 맞춘다.
// 부착 위치 자체는 bReplicateMovement의 부착 복제로 클라이언트에 전달된다.
// Large는 부착하지 않는다. 여러 명이 잡는 지점(GripPoints)을 하나씩 맡고, 필요 인원이 차면 들려서
// 운반자 모두가 같은 쪽으로 움직인 만큼만 움직인다. 위치는 서버가 계산해 이동 복제로 보낸다.
// 잡은 사람은 자기 지점에서 LeashLength 밖으로 못 나간다(이동 컴포넌트가 적용).

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
	Large // 인벤토리에 넣지 않고 여러 명이 잡는 지점을 맡아 함께 든다.
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

	/** Large 전용. 클라이언트 사전 확인과 서버 판정이 같은 규칙을 쓴다. 가까운 빈 잡는 지점이 있어야 한다. */
	bool CanAddLargeCarrier(const APawn* Carrier) const;

	/** Large 전용, 서버 전용. 가장 가까운 빈 잡는 지점을 맡긴다. 필요 인원이 차면 들린다. */
	bool AddLargeCarrier(APawn* Carrier);

	/** Large 전용, 서버 전용. 잡는 지점을 비우고 운반자의 인벤토리에 알린다. 인원이 모자라면 그 자리에 떨어진다. */
	void RemoveLargeCarrier(APawn* Carrier);

	/** Large가 들려서 운반자들을 따라가는 중인지. */
	bool IsLifted() const { return bLifted; }

	/** Large 전용. Carrier가 맡은 잡는 지점의 월드 위치. 맡은 지점이 없으면 false. */
	bool GetGripLocationFor(const APawn* Carrier, FVector& OutLocation) const;

	float GetLeashLength() const { return LeashLength; }

	virtual void Tick(float DeltaSeconds) override;

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

	/**
	 * Large 전용. 잡는 지점(화물 기준 위치). 배치한 인스턴스를 선택하면 뷰포트에서 끌어 옮길 수 있다.
	 * 개수가 곧 최대 인원이다. 양 끝에 두면 앞뒤로(소파 나르기), 좌우에 두면 나란히 든다.
	 */
	UPROPERTY(EditAnywhere, Category = "Cargo|Large",
		meta = (MakeEditWidget, EditCondition = "Weight == ESPCargoWeight::Large", EditConditionHides))
	TArray<FVector> GripPoints = { FVector(-100.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, 0.0f) };

	/** Large 전용. 중력에서 들리는 데 필요한 인원. */
	UPROPERTY(EditAnywhere, Category = "Cargo|Large",
		meta = (ClampMin = "1", EditCondition = "Weight == ESPCargoWeight::Large", EditConditionHides))
	int32 RequiredCarriers = 2;

	/** Large 전용. 무중력에서 들리는 데 필요한 인원. 들린 채 중력이 돌아와 인원이 모자라면 떨어진다. */
	UPROPERTY(EditAnywhere, Category = "Cargo|Large",
		meta = (ClampMin = "1", EditCondition = "Weight == ESPCargoWeight::Large", EditConditionHides))
	int32 ZeroGravityRequiredCarriers = 1;

	/** Large 전용. 캡슐 중심이 잡는 지점에서 이 거리(cm) 안에 있어야 잡을 수 있다. */
	UPROPERTY(EditAnywhere, Category = "Cargo|Large",
		meta = (ClampMin = "0.0", EditCondition = "Weight == ESPCargoWeight::Large", EditConditionHides))
	float GripRange = 150.0f;

	/**
	 * Large 전용. 잡은 뒤 자기 지점에서 벗어날 수 없는 거리(cm, 줄 길이). 중력에서는 수평, 무중력에서는 3D 거리로 묶는다.
	 * GripRange보다 짧으면 잡는 순간 이 거리까지 당겨 붙는다. 원격 클라이언트는 화물 위치를 늦게 보므로,
	 * 지연 동안 움직이는 거리(예: 100ms × 300cm/s = 30cm)보다 짧으면 줄 끝에서 위치 보정이 잦아진다.
	 */
	UPROPERTY(EditAnywhere, Category = "Cargo|Large",
		meta = (ClampMin = "0.0", EditCondition = "Weight == ESPCargoWeight::Large", EditConditionHides))
	float LeashLength = 150.0f;

	/**
	 * Large 전용. 순간이동이나 위치 보정으로 자기 지점에서 이 거리(cm)보다 멀어지면 놓친다. 평소에는 줄(LeashLength)에 막혀 닿지 않는다.
	 * GripRange와 LeashLength보다 커야 한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Cargo|Large",
		meta = (ClampMin = "0.0", EditCondition = "Weight == ESPCargoWeight::Large", EditConditionHides))
	float GripReleaseDistance = 250.0f;

	/** Large 전용. 들리는 순간 띄우는 높이(cm). 바닥에 닿은 채로 쓸면 이동이 막힌다. */
	UPROPERTY(EditAnywhere, Category = "Cargo|Large", AdvancedDisplay,
		meta = (ClampMin = "0.0", EditCondition = "Weight == ESPCargoWeight::Large", EditConditionHides))
	float LiftHeight = 10.0f;

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentCarrier)
	TObjectPtr<APawn> CurrentCarrier;

	UFUNCTION()
	void OnRep_CurrentCarrier();

	/** Large 전용. GripPoints와 같은 순서의 운반자이고, 빈 지점은 null이다. 클라이언트 사전 확인에도 쓰므로 복제한다. */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<APawn>> GripCarriers;

	UPROPERTY(ReplicatedUsing = OnRep_Lifted)
	bool bLifted = false;

	UFUNCTION()
	void OnRep_Lifted();

	/** 서버 전용. GripCarriers와 같은 순서로, 잡을 때(또는 들릴 때) 잡는 지점에서 본 운반자 위치. 여기서 벗어난 만큼이 당긴 양이다. */
	TArray<FVector> GripOffsets;

	void StartPhysicsFromServerState();

	int32 FindFreeGripPoint(const APawn& Carrier) const;
	FVector GetGripLocation(int32 Index) const;
	int32 CountCarriers() const;
	bool IsInZeroGravity() const;
	void RefreshLift();
	void ResetGripOffsets();
	void ReleaseDistantCarriers();
	void MoveWithCarriers();
};
