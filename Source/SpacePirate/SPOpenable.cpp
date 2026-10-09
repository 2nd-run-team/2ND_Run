#include "SPOpenable.h"

#include "SPCargo.h"
#include "SPInteractableComponent.h"
#include "SPInventoryComponent.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

ASPOpenable::ASPOpenable()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
    SetRootComponent(BodyMesh);

    DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
    DoorMesh->SetupAttachment(BodyMesh);

    ContentsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ContentsBox"));
    ContentsBox->SetupAttachment(BodyMesh);
    ContentsBox->SetBoxExtent(FVector::ZeroVector);
    ContentsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Interactable = CreateDefaultSubobject<USPInteractableComponent>(TEXT("Interactable"));
    Interactable->Prompt = NSLOCTEXT("SpacePirate", "OpenPrompt", "열기");
}

void ASPOpenable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASPOpenable, bOpen);
}

void ASPOpenable::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // 늦게 접속한 클라이언트의 OnRep보다 먼저 실행된다.
    ClosedDoorLocation = DoorMesh->GetRelativeLocation();
}

void ASPOpenable::BeginPlay()
{
    Super::BeginPlay();

    // 클라이언트도 요청 전에 같은 조건으로 미리 거르므로 양쪽에서 묶는다. 완료 알림은 서버에서만 온다.
    Interactable->CanInteractNative.BindUObject(this, &ASPOpenable::CanOpen);
    Interactable->OnCompletedNative.AddUObject(this, &ASPOpenable::HandleOpened);

    if (HasAuthority())
    {
        MarkContents();
    }
}

bool ASPOpenable::CanOpen(APawn* User) const
{
    if (bOpen)
    {
        return false;
    }
    if (!bRequiresHandItem)
    {
        return true;
    }

    const USPInventoryComponent* Inventory = User ? User->FindComponentByClass<USPInventoryComponent>() : nullptr;
    const ASPCargo* HandItem = Inventory ? Inventory->GetHandItem() : nullptr;
    return HandItem && HandItem->GetItemType() == RequiredHandItem;
}

void ASPOpenable::HandleOpened(APawn* User)
{
    Open();
}

void ASPOpenable::Open()
{
    if (!HasAuthority() || bOpen)
    {
        return;
    }

    bOpen = true;
    OnRep_Open();
}

void ASPOpenable::OnRep_Open()
{
    if (!bOpen)
    {
        return;
    }

    // ponytail: 문짝을 즉시 옮긴다. 열림 연출이 필요해지면 BP 이벤트나 타임라인으로 바꾼다.
    DoorMesh->SetRelativeLocation(ClosedDoorLocation + OpenOffset);

    // 열린 뒤 E를 눌러도 "키카드 필요" 같은 거절 안내가 뜨지 않게 한다.
    Interactable->BlockedPrompt = FText::GetEmpty();
}

void ASPOpenable::MarkContents()
{
    const FVector Extent = ContentsBox->GetUnscaledBoxExtent();
    if (Extent.IsNearlyZero())
    {
        return;
    }

    // 레벨에서는 물건을 안에 놓기만 하면 된다. 회전한 보관함도 맞도록 상자 기준 좌표로 비교한다.
    const FBox LocalBox(-Extent, Extent);
    const FTransform& BoxTransform = ContentsBox->GetComponentTransform();
    for (TActorIterator<ASPCargo> It(GetWorld()); It; ++It)
    {
        if (!It->IsCarried() && LocalBox.IsInsideOrOn(BoxTransform.InverseTransformPosition(It->GetActorLocation())))
        {
            It->MarkStoredInContainer();
        }
    }
}
