// Fill out your copyright notice in the Description page of Project Settings.


#include "SPCargo.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ASPCargo::ASPCargo()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	CargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
	SetRootComponent(CargoMesh);
}

void ASPCargo::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPCargo, CurrentCarrier);
}

// Called when the game starts or when spawned
void ASPCargo::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("Cargo spawned: %s"), *GetName());
}

bool ASPCargo::AttachToCarrier(APawn* Carrier, USceneComponent* HoldPoint)
{
	if (!ensure(HasAuthority()) || IsCarried() || !Carrier || !HoldPoint)
	{
		return false;
	}

	if (!AttachToComponent(HoldPoint,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		return false;
	}

	SetActorRelativeLocation(CarryLocationOffset);
	SetActorRelativeRotation(CarryRotationOffset);

	CurrentCarrier = Carrier;
	// 서버에서는 RepNotify가 자동 호출되지 않는다.
	OnRep_CurrentCarrier();
	return true;
}

void ASPCargo::OnRep_CurrentCarrier()
{
	// 운반 중 화물이 운반자 캡슐 이동이나 다른 플레이어의 트레이스를 막지 않게 한다.
	// ponytail: 내려놓기가 없어 복원 분기 생략. Drop 추가 시 원래 충돌 설정을 되돌린다.
	if (IsCarried())
	{
		CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}
