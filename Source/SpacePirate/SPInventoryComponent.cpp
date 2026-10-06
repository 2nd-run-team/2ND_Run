#include "SPInventoryComponent.h"

#include "SPDebug.h"
#include "SPCargo.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

USPInventoryComponent::USPInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void USPInventoryComponent::PostInitProperties()
{
    Super::PostInitProperties();

    // 생성자 시점에는 C++ 기본값만 보이므로, BP에서 바꾼 SlotCount가 적용된 뒤에 칸을 만든다.
    Slots.SetNum(FMath::Max(SlotCount, 1));
}

void USPInventoryComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 다른 플레이어 화면의 운반 포즈와 이동속도 계산도 슬롯을 보므로 모두에게 복제한다.
    DOREPLIFETIME(USPInventoryComponent, Slots);
    DOREPLIFETIME(USPInventoryComponent, ActiveSlot);
}

ASPCargo* USPInventoryComponent::GetActiveItem() const
{
    return Slots.IsValidIndex(ActiveSlot) && IsValid(Slots[ActiveSlot])
        ? Slots[ActiveSlot].Get()
        : nullptr;
}

ASPCargo* USPInventoryComponent::GetHandItem() const
{
    ASPCargo* Item = GetActiveItem();
    return Item && !Item->IsBag() ? Item : nullptr;
}

bool USPInventoryComponent::HasBag() const
{
    return Slots.ContainsByPredicate(
        [](const TObjectPtr<ASPCargo>& Item) { return IsValid(Item) && Item->IsBag(); });
}

int32 USPInventoryComponent::FindSlotForPickUp() const
{
    // 현재 칸이 비어 있으면 그 칸, 아니면 첫 빈칸에 넣는다.
    if (Slots.IsValidIndex(ActiveSlot) && !IsValid(Slots[ActiveSlot]))
    {
        return ActiveSlot;
    }

    return Slots.IndexOfByPredicate(
        [](const TObjectPtr<ASPCargo>& Item) { return !IsValid(Item); });
}

bool USPInventoryComponent::CanPickUp(const ASPCargo* Item) const
{
    return IsValid(Item)
        && !Item->IsCarried()
        && !(Item->IsBag() && HasBag())
        && FindSlotForPickUp() != INDEX_NONE;
}

void USPInventoryComponent::PickUp(ASPCargo* Item)
{
    APawn* OwnerPawn = GetOwner<APawn>();

    // 동시 요청의 패배, 가득 참, 가방 중복은 정상 흐름이라 기록하지 않는다.
    if (!ensure(OwnerPawn && OwnerPawn->HasAuthority()) || !CanPickUp(Item))
    {
        return;
    }

    USceneComponent* AttachPoint = Item->IsBag() ? BackPoint : HandPoint;
    if (!Item->AttachToCarrier(OwnerPawn, AttachPoint))
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Pickup failed: could not attach %s to %s. Check that the owner calls SetAttachPoints in its constructor."),
            *OwnerPawn->GetName(), *Item->GetName(), *GetNameSafe(AttachPoint));
        return;
    }

    const int32 SlotIndex = FindSlotForPickUp();
    Slots[SlotIndex] = Item;
    ApplyActiveSlot(SlotIndex);
}

void USPInventoryComponent::ServerDrop_Implementation()
{
    DropActiveItem(FVector::ZeroVector);
}

void USPInventoryComponent::ServerThrow_Implementation(float Charge)
{
    const ASPCargo* Item = GetActiveItem();
    const APawn* OwnerPawn = GetOwner<APawn>();

    // 도구는 길게 눌러도 내려놓는다.
    if (!Item || !Item->IsBag() || !OwnerPawn)
    {
        DropActiveItem(FVector::ZeroVector);
        return;
    }

    // 원격 플레이어의 시선 상하 각도는 GetBaseAimRotation이 복제된 값으로 채운다.
    const float Speed = FMath::Lerp(MinThrowSpeed, MaxThrowSpeed, FMath::Clamp(Charge, 0.0f, 1.0f));
    DropActiveItem(OwnerPawn->GetBaseAimRotation().Vector() * Speed);
}

void USPInventoryComponent::DropActiveItem(const FVector& Velocity)
{
    ASPCargo* Item = GetActiveItem();
    FVector DropLocation;

    // 앞이 막혀 있으면 계속 든다. 벽에 붙어 누른 정상 상황이라 기록하지 않는다.
    if (!Item || !FindDropLocation(*Item, DropLocation))
    {
        return;
    }

    Item->DetachFromCarrier(DropLocation, Velocity);
    Slots[ActiveSlot] = nullptr;

    // 서버에서는 RepNotify가 자동 호출되지 않는다.
    OnRep_Inventory();
}

void USPInventoryComponent::ServerSelectSlot_Implementation(int32 SlotIndex)
{
    if (!Slots.IsValidIndex(SlotIndex) || SlotIndex == ActiveSlot)
    {
        return;
    }

    ApplyActiveSlot(SlotIndex);
}

void USPInventoryComponent::ServerCycleSlot_Implementation(int32 Direction)
{
    if (Direction == 0)
    {
        return;
    }

    const int32 Step = Direction > 0 ? 1 : -1;
    const int32 NumSlots = Slots.Num();
    ServerSelectSlot_Implementation((ActiveSlot + Step + NumSlots) % NumSlots);
}

void USPInventoryComponent::ApplyActiveSlot(int32 NewSlot)
{
    ActiveSlot = NewSlot;

    // 가방은 등에 항상 보이고, 손 물건은 현재 칸만 보인다. bHidden은 복제되므로 서버에서만 바꾸면 된다.
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        if (ASPCargo* Item = Slots[Index]; IsValid(Item))
        {
            Item->SetActorHiddenInGame(!Item->IsBag() && Index != ActiveSlot);
        }
    }

    // 서버에서는 RepNotify가 자동 호출되지 않는다.
    OnRep_Inventory();
}

bool USPInventoryComponent::FindDropLocation(
    const ASPCargo& Item,
    FVector& OutLocation) const
{
    const AActor* Owner = GetOwner();

    // 운반 중에는 충돌이 꺼져 있으므로 충돌 없는 컴포넌트까지 포함해 크기를 잰다.
    const FBox ItemBounds = Item.GetComponentsBoundingBox(true);
    const FVector ItemExtent = ItemBounds.GetExtent();

    float OwnerRadius = 0.0f;
    float OwnerHalfHeight = 0.0f;
    Owner->GetSimpleCollisionCylinder(OwnerRadius, OwnerHalfHeight);

    // 소유자 중심에서 몸 정면으로, 물건이 소유자와 겹치지 않는 거리까지 물건 크기의 상자를 쓸어 본다.
    const FVector Start = Owner->GetActorLocation();
    const float Distance = OwnerRadius + ItemExtent.Size2D() + DropGap;
    const FVector End = Start + Owner->GetActorForwardVector() * Distance;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SPItemDrop), false, Owner);
    Params.AddIgnoredActor(&Item);

    FHitResult Hit;
    const bool bHit = GetWorld()->SweepSingleByChannel(
        Hit,
        Start,
        End,
        FQuat::Identity,
        DropTraceChannel,
        FCollisionShape::MakeBox(ItemExtent),
        Params);

    // 시작부터 겹치면(벽에 붙음, 물건이 소유자보다 큼) 놓을 자리가 없다.
    if (bHit && Hit.bStartPenetrating)
    {
        return false;
    }

    const FVector DropCenter = bHit ? Hit.Location : End;

    // 상자 중심 기준으로 찾았으므로 메시 피벗 위치로 되돌린다.
    OutLocation = DropCenter
        + (Item.GetActorLocation() - ItemBounds.GetCenter());
    return true;
}

void USPInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 소유자가 파괴(접속 종료 등)되면 가진 물건이 숨겨지거나 충돌 없는 상태로 남지 않게 제자리에 떨어뜨린다.
    // 레벨 종료 시에는 물건도 함께 정리되므로 처리하지 않는다.
    if (EndPlayReason == EEndPlayReason::Destroyed
        && GetOwner() && GetOwner()->HasAuthority())
    {
        DropAll();
    }

    Super::EndPlay(EndPlayReason);
}

ASPCargo* USPInventoryComponent::FindItemOfType(ESPItemType Type) const
{
    const TObjectPtr<ASPCargo>* Found = Slots.FindByPredicate(
        [Type](const TObjectPtr<ASPCargo>& Item) { return IsValid(Item) && Item->GetItemType() == Type; });
    return Found ? Found->Get() : nullptr;
}

void USPInventoryComponent::ConsumeItem(ASPCargo* Item)
{
    const int32 SlotIndex = Item ? Slots.IndexOfByKey(Item) : INDEX_NONE;
    if (!ensure(GetOwner() && GetOwner()->HasAuthority()) || SlotIndex == INDEX_NONE)
    {
        return;
    }

    Slots[SlotIndex] = nullptr;
    // 서버에서는 RepNotify가 자동 호출되지 않는다.
    OnRep_Inventory();
    Item->Destroy();
}

void USPInventoryComponent::DropBag()
{
    if (!ensure(GetOwner() && GetOwner()->HasAuthority()))
    {
        return;
    }

    for (TObjectPtr<ASPCargo>& Item : Slots)
    {
        if (IsValid(Item) && Item->IsBag())
        {
            // 쓰러진 자리에 떨어지도록 등에 붙어 있던 위치에서 물리를 시작한다.
            Item->DetachFromCarrier(Item->GetActorLocation());
            Item = nullptr;
            OnRep_Inventory();
            return;
        }
    }
}

void USPInventoryComponent::DropAll()
{
    for (TObjectPtr<ASPCargo>& Item : Slots)
    {
        if (IsValid(Item))
        {
            Item->DetachFromCarrier(Item->GetActorLocation());
        }

        Item = nullptr;
    }
}

void USPInventoryComponent::OnRep_Inventory()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    // [TEMP-INVENTORY-DEBUG] 핫바 HUD가 생기면 이 표시를 제거한다.
    // PIE 창들은 GEngine을 공유하므로 소유자 이름으로 구분한다.
    const APawn* OwnerPawn = GetOwner<APawn>();
    if (!GEngine || !OwnerPawn || !OwnerPawn->IsLocallyControlled())
    {
        return;
    }

    FString Text = OwnerPawn->GetName() + TEXT("  ");
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        const ASPCargo* Item = Slots[Index];
        const FString ItemName = IsValid(Item)
            ? UEnum::GetDisplayValueAsText(Item->GetItemType()).ToString()
            : TEXT("-");

        Text += FString::Printf(TEXT("%s[%d] %s  "),
            Index == ActiveSlot ? TEXT(">") : TEXT(""),
            Index + 1,
            *ItemName);
    }

    GEngine->AddOnScreenDebugMessage(
        static_cast<uint64>(GetUniqueID()), 3600.0f, FColor::Cyan, Text);
#endif
}
