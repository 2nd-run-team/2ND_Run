#include "SPCargo.h"

#include "SPInteractableComponent.h"
#include "SPInventoryComponent.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ASPCargo::ASPCargo()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	// 운반 중에는 소유자(운반자)의 관련성을 따른다. 인벤토리에 숨겨 둔 물건은 숨김+충돌 없음이라
	// 기본 규칙으로는 다른 플레이어에게 복제가 끊겨, 숨김/전환 상태가 전달되지 않는다.
	bNetUseOwnerRelevancy = true;

	CargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
	SetRootComponent(CargoMesh);

	Interactable = CreateDefaultSubobject<USPInteractableComponent>(TEXT("Interactable"));
	Interactable->Prompt = NSLOCTEXT("SpacePirate", "PickUpPrompt", "줍기");
}

void ASPCargo::BeginPlay()
{
	Super::BeginPlay();

	// 클라이언트도 요청 전에 같은 조건으로 미리 거르므로 양쪽에서 묶는다.
	Interactable->CanInteractNative.BindUObject(this, &ASPCargo::CanBePickedUpBy);
	Interactable->OnCompletedNative.AddUObject(this, &ASPCargo::HandlePickedUp);
}

bool ASPCargo::CanBePickedUpBy(APawn* User) const
{
	const USPInventoryComponent* Inventory = User ? User->FindComponentByClass<USPInventoryComponent>() : nullptr;
	return Inventory && Inventory->CanPickUp(this);
}

void ASPCargo::HandlePickedUp(APawn* User)
{
	// 범죄 보고는 완료 알림보다 먼저 끝나므로 여기서 지워도 이번 줍기는 범죄로 남는다.
	Interactable->CrimeKind = ESPCrimeKind::None;

	if (USPInventoryComponent* Inventory = User ? User->FindComponentByClass<USPInventoryComponent>() : nullptr)
	{
		Inventory->PickUp(this);
	}
}

void ASPCargo::MarkStoredInContainer()
{
	if (HasAuthority())
	{
		Interactable->CrimeKind = ESPCrimeKind::ContainerTheft;
		Interactable->bInstantCrime = true;
	}
}

void ASPCargo::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASPCargo, CurrentCarrier);
}

bool ASPCargo::AttachToCarrier(APawn* Carrier, USceneComponent* AttachPoint)
{
	if (!ensure(HasAuthority()) || IsCarried() || !Carrier || !AttachPoint)
	{
		return false;
	}

	// 물리 시뮬레이션 중인 바디는 부착에서 떨어져 나가므로, 운반 상태(물리 끔)를 먼저 적용한다.
	// 서버에서는 RepNotify가 자동 호출되지 않는다.
	CurrentCarrier = Carrier;
	OnRep_CurrentCarrier();
	SetOwner(Carrier);

	if (!AttachToComponent(AttachPoint,
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

bool ASPCargo::DetachFromCarrier(const FVector& DropLocation, const FVector& Velocity)
{
	// 운반자가 파괴되는 중에도 호출되므로 IsValid가 아닌 null로 판단한다.
	if (!ensure(HasAuthority()) || !CurrentCarrier)
	{
		return false;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorLocation(DropLocation, false, nullptr, ETeleportType::TeleportPhysics);
	// 인벤토리에 보관 중이던 물건은 숨겨져 있을 수 있다.
	SetActorHiddenInGame(false);
	SetOwner(nullptr);

	CurrentCarrier = nullptr;
	OnRep_CurrentCarrier();

	// 물리가 켜진 뒤에 속도를 준다. 클라이언트는 이동 복제로 받은 속도에서 시작한다.
	if (!Velocity.IsZero())
	{
		CargoMesh->SetPhysicsLinearVelocity(Velocity);
	}
	return true;
}

void ASPCargo::OnRep_AttachmentReplication()
{
	// 부착 정보(AActor 속성)는 CurrentCarrier보다 먼저 처리된다. 떨어뜨려 물리 시뮬레이션 중인 물건은
	// 그대로 붙이면 물리가 위치를 덮어써 바닥에 남으므로, 붙기 전에 물리부터 끈다.
	if (GetAttachmentReplication().AttachParent)
	{
		CargoMesh->SetSimulatePhysics(false);
	}

	Super::OnRep_AttachmentReplication();
}

void ASPCargo::OnRep_CurrentCarrier()
{
	// 등 가방은 1인칭 카메라 바로 뒤에 붙어 화면에 끼어들므로 멘 사람에게만 숨긴다. 소유자가 곧 운반자다.
	CargoMesh->SetOwnerNoSee(IsCarried() && IsBag());

	if (IsCarried())
	{
		// 운반 중 물건이 운반자 캡슐 이동이나 다른 플레이어의 트레이스를 막지 않게 한다.
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
