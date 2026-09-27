// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// 역할: 운반 가능한 화물. 운반자 지정과 부착은 서버에서만 하고, 운반자는 복제되어 클라이언트 충돌을 맞춘다.
// 부착 위치 자체는 bReplicateMovement의 부착 복제로 클라이언트에 전달된다.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPCargo.generated.h"

class APawn;
class UStaticMeshComponent;

UCLASS()
class SECONDRUN_API ASPCargo : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ASPCargo();

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. 운반자의 HoldPoint에 붙이고 충돌을 끈다. 이미 운반 중이면 실패한다. */
	bool AttachToCarrier(APawn* Carrier, USceneComponent* HoldPoint);

	bool IsCarried() const { return IsValid(CurrentCarrier); }

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

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentCarrier)
	TObjectPtr<APawn> CurrentCarrier;

	UFUNCTION()
	void OnRep_CurrentCarrier();
};
