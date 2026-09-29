// Fill out your copyright notice in the Description page of Project Settings.


#include "SPCargo.h"

#include "SPGravityWorldSubsystem.h"
#include "SPInventoryComponent.h"

#include "Algo/Count.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

// Sets default values
ASPCargo::ASPCargo()
{
	// Large를 잡고 있는 동안에만 서버에서 켠다. 운반자들이 이번 프레임 이동을 마친 뒤 따라가도록 물리 이후에 돈다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	bReplicates = true;
	SetReplicateMovement(true);

	// 운반 중에는 소유자(운반자)의 관련성을 따른다. 인벤토리에 숨겨 둔 화물은 숨김+충돌 없음이라
	// 기본 규칙으로는 다른 플레이어에게 복제가 끊겨, 숨김/전환 상태가 전달되지 않는다.
	bNetUseOwnerRelevancy = true;

	CargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
	SetRootComponent(CargoMesh);
}

void ASPCargo::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPCargo, CurrentCarrier);
	DOREPLIFETIME(ASPCargo, GripCarriers);
	DOREPLIFETIME(ASPCargo, bLifted);
}

// Called when the game starts or when spawned
void ASPCargo::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("Cargo spawned: %s"), *GetName());

	// 배치한 인스턴스에서 바꾼 GripPoints까지 적용된 뒤라서 여기서 칸을 만든다. 클라이언트는 복제로 받는다.
	if (HasAuthority() && Weight == ESPCargoWeight::Large)
	{
		GripCarriers.SetNum(GripPoints.Num());
		GripOffsets.SetNum(GripPoints.Num());
	}
}

bool ASPCargo::AttachToCarrier(APawn* Carrier, USceneComponent* HoldPoint)
{
	if (!ensure(HasAuthority()) || IsCarried() || !Carrier || !HoldPoint)
	{
		return false;
	}

	// 물리 시뮬레이션 중인 바디는 부착에서 떨어져 나가므로, 운반 상태(물리 끔)를 먼저 적용한다.
	// 서버에서는 RepNotify가 자동 호출되지 않는다.
	CurrentCarrier = Carrier;
	OnRep_CurrentCarrier();
	SetOwner(Carrier);

	if (!AttachToComponent(HoldPoint,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		CurrentCarrier = nullptr;
		OnRep_CurrentCarrier();
		SetOwner(nullptr);
		return false;
	}

	SetActorRelativeLocation(CarryLocationOffset);
	SetActorRelativeRotation(CarryRotationOffset);
	return true;
}

bool ASPCargo::DetachFromCarrier(const FVector& DropLocation)
{
	// 운반자가 파괴되는 중에도 호출되므로 IsValid가 아닌 null로 판단한다.
	if (!ensure(HasAuthority()) || !CurrentCarrier)
	{
		return false;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
	// 인벤토리에 보관 중이던 화물은 숨겨져 있을 수 있다.
	SetActorHiddenInGame(false);
	SetOwner(nullptr);

	CurrentCarrier = nullptr;
	OnRep_CurrentCarrier();
	return true;
}

void ASPCargo::OnRep_AttachmentReplication()
{
	// 부착 정보(AActor 속성)는 CurrentCarrier보다 먼저 처리된다. 떨어뜨려 물리 시뮬레이션 중인 화물은
	// 그대로 붙이면 물리가 위치를 덮어써 바닥에 남으므로, 붙기 전에 물리부터 끈다.
	if (GetAttachmentReplication().AttachParent)
	{
		CargoMesh->SetSimulatePhysics(false);
	}

	Super::OnRep_AttachmentReplication();
}

void ASPCargo::OnRep_CurrentCarrier()
{
	if (IsCarried())
	{
		// 운반 중 화물이 운반자 캡슐 이동이나 다른 플레이어의 트레이스를 막지 않게 한다.
		CargoMesh->SetSimulatePhysics(false);
		CargoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	StartPhysicsFromServerState();
}

void ASPCargo::StartPhysicsFromServerState()
{
	// 서버는 버릴 자리로 순간이동시킨 뒤 물리를 켠다. 클라이언트는 손에서 떼어진 위치에 있으므로
	// 서버가 보낸 위치·속도에서 시작해야 낙하 중 보정과 멈출 때의 튐이 생기지 않는다.
	const bool bIsClient = !HasAuthority();
	if (bIsClient)
	{
		const FRepMovement& ServerMovement = GetReplicatedMovement();
		SetActorLocationAndRotation(
			ServerMovement.Location,
			ServerMovement.Rotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	// 집기 트레이스(Query)와 낙하(Physics)에 둘 다 필요하다. 물리를 켜기 전에 충돌부터 켠다.
	// 클라이언트도 시뮬레이션해야 서버 물리 상태를 부드럽게 따라간다.
	CargoMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CargoMesh->SetSimulatePhysics(true);

	if (bIsClient)
	{
		CargoMesh->SetPhysicsLinearVelocity(GetReplicatedMovement().LinearVelocity);
	}
}

bool ASPCargo::CanAddLargeCarrier(const APawn* Carrier) const
{
	return Weight == ESPCargoWeight::Large
		&& IsValid(Carrier)
		&& !GripCarriers.Contains(Carrier)
		&& FindFreeGripPoint(*Carrier) != INDEX_NONE;
}

bool ASPCargo::AddLargeCarrier(APawn* Carrier)
{
	if (!ensure(HasAuthority()) || !CanAddLargeCarrier(Carrier))
	{
		return false;
	}

	const int32 Index = FindFreeGripPoint(*Carrier);
	GripCarriers[Index] = Carrier;
	GripOffsets[Index] = Carrier->GetActorLocation() - GetGripLocation(Index);
	RefreshLift();
	return true;
}

void ASPCargo::RemoveLargeCarrier(APawn* Carrier)
{
	// 빈 지점도 null이라 null로 찾으면 빈 지점이 잡힌다.
	const int32 Index = Carrier ? GripCarriers.Find(Carrier) : INDEX_NONE;
	if (!ensure(HasAuthority()) || Index == INDEX_NONE)
	{
		return;
	}

	GripCarriers[Index] = nullptr;

	// 놓는 이유(버리기 키, 접속 종료, 너무 멀어짐)와 상관없이 운반자 쪽 상태를 여기서 함께 푼다.
	if (USPInventoryComponent* Inventory = Carrier->FindComponentByClass<USPInventoryComponent>())
	{
		Inventory->OnLargeReleased(this);
	}

	RefreshLift();
}

bool ASPCargo::GetGripLocationFor(const APawn* Carrier, FVector& OutLocation) const
{
	const int32 Index = Carrier ? GripCarriers.IndexOfByKey(Carrier) : INDEX_NONE;
	if (!GripPoints.IsValidIndex(Index))
	{
		return false;
	}

	OutLocation = GetGripLocation(Index);
	return true;
}

void ASPCargo::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ReleaseDistantCarriers();

	// 들고 가는 도중 중력이 바뀌면 필요 인원도 바뀐다.
	RefreshLift();

	if (bLifted)
	{
		MoveWithCarriers();
	}
}

void ASPCargo::MoveWithCarriers()
{
	// 원격 플레이어의 이동은 몇 프레임씩 몰아서 도착한다. 프레임마다의 이동량이 아니라 잡은 뒤 누적으로 당긴 양을 봐야
	// 늦게 도착한 쪽이 따라잡을 때 화물도 그만큼 따라간다.
	TArray<FVector, TInlineAllocator<4>> Pulls;
	FVector PullSum = FVector::ZeroVector;

	for (int32 Index = 0; Index < GripCarriers.Num(); ++Index)
	{
		if (const APawn* Carrier = GripCarriers[Index]; IsValid(Carrier))
		{
			const FVector Pull =
				Carrier->GetActorLocation() - GetGripLocation(Index) - GripOffsets[Index];
			Pulls.Add(Pull);
			PullSum += Pull;
		}
	}

	// 모두가 같은 쪽으로 당긴 만큼만 움직인다. 한 명만 가거나 서로 반대로 가면 움직이지 않고,
	// 속도가 다르면 느린 쪽에 맞춰진다. 앞서간 쪽은 이동 컴포넌트의 줄(LeashLength)에 막힌다.
	const FVector Direction = PullSum.GetSafeNormal();
	if (Direction.IsZero())
	{
		return;
	}

	double CommonPull = TNumericLimits<double>::Max();
	for (const FVector& Pull : Pulls)
	{
		CommonPull = FMath::Min(CommonPull, FVector::DotProduct(Pull, Direction));
	}

	if (CommonPull <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	// 수평과 수직을 나눠 쓸어야 바닥에 막힌 수직 이동이 수평 이동까지 막지 않는다.
	// 벽 등에 막히면 그 자리에 멈추고, 운반자는 줄에 막혀 함께 멈춘다.
	const FVector Move = Direction * CommonPull;
	AddActorWorldOffset(FVector(Move.X, Move.Y, 0.0), true);
	AddActorWorldOffset(FVector(0.0, 0.0, Move.Z), true);
}

void ASPCargo::ReleaseDistantCarriers()
{
	const float MaxDistanceSquared = FMath::Square(GripReleaseDistance);

	for (int32 Index = 0; Index < GripCarriers.Num(); ++Index)
	{
		APawn* Carrier = GripCarriers[Index];
		if (Carrier && (!IsValid(Carrier)
			|| FVector::DistSquared(Carrier->GetActorLocation(), GetGripLocation(Index)) > MaxDistanceSquared))
		{
			RemoveLargeCarrier(Carrier);
		}
	}
}

void ASPCargo::RefreshLift()
{
	const int32 NumCarriers = CountCarriers();
	const int32 RequiredNow = FMath::Max(
		IsInZeroGravity() ? ZeroGravityRequiredCarriers : RequiredCarriers, 1);
	const bool bShouldLift = NumCarriers >= RequiredNow;

	if (bShouldLift != bLifted)
	{
		bLifted = bShouldLift;
		// 서버에서는 RepNotify가 자동 호출되지 않는다.
		OnRep_Lifted();

		if (bLifted)
		{
			AddActorWorldOffset(FVector(0.0f, 0.0f, LiftHeight), true);

			// 들리기 전에 화물이 구르거나 떨어졌을 수 있다. 지금 자리를 기준으로 삼아 들리자마자 끌려가지 않게 한다.
			ResetGripOffsets();
		}
	}

	SetActorTickEnabled(NumCarriers > 0);
}

void ASPCargo::ResetGripOffsets()
{
	for (int32 Index = 0; Index < GripCarriers.Num(); ++Index)
	{
		if (const APawn* Carrier = GripCarriers[Index]; IsValid(Carrier))
		{
			GripOffsets[Index] = Carrier->GetActorLocation() - GetGripLocation(Index);
		}
	}
}

void ASPCargo::OnRep_Lifted()
{
	// 운반자 캡슐과 서로 막으면 화물이 운반자 사이에 끼어 움직이지 못한다. 클라이언트 이동 예측도 같은 충돌을 봐야 해서 복제로 맞춘다.
	// ponytail: 들린 동안에는 운반자가 아닌 플레이어도 통과한다. 운반자만 통과시켜야 하면 운반자 캡슐마다 IgnoreActorWhenMoving을 쓴다.
	// 내려놓으면 Block으로 되돌리므로, 화물 BP의 Pawn 응답은 Block이어야 한다.
	CargoMesh->SetCollisionResponseToChannel(ECC_Pawn, bLifted ? ECR_Ignore : ECR_Block);

	if (bLifted)
	{
		// 들린 동안에는 서버가 위치를 정해 이동 복제로 보낸다.
		CargoMesh->SetSimulatePhysics(false);
		return;
	}

	StartPhysicsFromServerState();
}

int32 ASPCargo::FindFreeGripPoint(const APawn& Carrier) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistanceSquared = FMath::Square(GripRange);

	// GripCarriers는 서버 BeginPlay에서 GripPoints 개수로 만들어 복제된다. 도착 전에는 비어 있다.
	const int32 NumPoints = FMath::Min(GripPoints.Num(), GripCarriers.Num());
	for (int32 Index = 0; Index < NumPoints; ++Index)
	{
		if (IsValid(GripCarriers[Index]))
		{
			continue;
		}

		const float DistanceSquared =
			FVector::DistSquared(Carrier.GetActorLocation(), GetGripLocation(Index));
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestIndex = Index;
			BestDistanceSquared = DistanceSquared;
		}
	}

	return BestIndex;
}

FVector ASPCargo::GetGripLocation(int32 Index) const
{
	return GetActorTransform().TransformPosition(GripPoints[Index]);
}

int32 ASPCargo::CountCarriers() const
{
	return Algo::CountIf(GripCarriers,
		[](const TObjectPtr<APawn>& Carrier) { return IsValid(Carrier); });
}

bool ASPCargo::IsInZeroGravity() const
{
	// 운반자들이 서로 다른 영역에 걸쳐 있어도 화물 위치 하나로 판정한다. 영역은 서버에만 등록되고, 영역 밖은 무중력이다.
	const USPGravityWorldSubsystem* Gravity = GetWorld()->GetSubsystem<USPGravityWorldSubsystem>();
	return !Gravity
		|| Gravity->GetGravityModeAtLocation(GetActorLocation()) == ESPGravityMode::ZeroGravity;
}
