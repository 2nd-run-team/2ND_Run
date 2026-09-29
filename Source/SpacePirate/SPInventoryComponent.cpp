#include "SPInventoryComponent.h"

#include "SPCargo.h"
#include "SPDebug.h"

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
    DOREPLIFETIME(USPInventoryComponent, GrippedLarge);
}

ASPCargo* USPInventoryComponent::GetGrippedLarge() const
{
    return IsValid(GrippedLarge) ? GrippedLarge.Get() : nullptr;
}

void USPInventoryComponent::OnLargeReleased(const ASPCargo* Cargo)
{
    if (GrippedLarge.Get() == Cargo)
    {
        GrippedLarge = nullptr;

        // 서버에서는 RepNotify가 자동 호출되지 않는다.
        OnRep_Inventory();
    }
}

ASPCargo* USPInventoryComponent::GetActiveCargo() const
{
    return Slots.IsValidIndex(ActiveSlot) && IsValid(Slots[ActiveSlot])
        ? Slots[ActiveSlot].Get()
        : nullptr;
}

bool USPInventoryComponent::IsHoldingTwoHanded() const
{
    const ASPCargo* ActiveCargo = GetActiveCargo();
    return ActiveCargo && ActiveCargo->IsTwoHanded();
}

int32 USPInventoryComponent::FindSlotForPickUp() const
{
    // 현재 칸이 비어 있으면 그 칸, 아니면 첫 빈칸에 넣는다.
    if (Slots.IsValidIndex(ActiveSlot) && !IsValid(Slots[ActiveSlot]))
    {
        return ActiveSlot;
    }

    return Slots.IndexOfByPredicate(
        [](const TObjectPtr<ASPCargo>& Cargo) { return !IsValid(Cargo); });
}

bool USPInventoryComponent::CanPickUp(const ASPCargo* Cargo) const
{
    if (!IsValid(Cargo) || IsGrippingLarge())
    {
        return false;
    }

    // Large는 칸에 넣지 않고 잡는 지점을 맡는다. 양손이 필요하므로 현재 칸이 비어 있어야 한다.
    if (Cargo->GetWeight() == ESPCargoWeight::Large)
    {
        return !GetActiveCargo() && Cargo->CanAddLargeCarrier(GetOwner<APawn>());
    }

    return !Cargo->IsCarried()
        && !IsHoldingTwoHanded()
        && FindSlotForPickUp() != INDEX_NONE;
}

void USPInventoryComponent::ServerPickUp_Implementation(ASPCargo* Cargo)
{
    APawn* OwnerPawn = GetOwner<APawn>();

    // 동시 요청의 패배, 가득 참, 양손 제한은 정상 흐름이라 기록하지 않는다.
    if (!OwnerPawn || !CanPickUp(Cargo))
    {
        return;
    }

    // Large는 CanPickUp에서 잡는 지점까지의 거리를 이미 확인했다.
    if (Cargo->GetWeight() == ESPCargoWeight::Large)
    {
        if (Cargo->AddLargeCarrier(OwnerPawn))
        {
            GrippedLarge = Cargo;
            OnRep_Inventory();
        }
        return;
    }

    const float Distance = OwnerPawn->GetDistanceTo(Cargo);
    if (Distance > ServerPickupRange)
    {
        SP_DEBUG_LOG(Warning, TEXT("%s: Pickup rejected: %s is %.0f away (limit %.0f). Raise ServerPickupRange if this happens under normal latency."),
            *OwnerPawn->GetName(), *Cargo->GetName(), Distance, ServerPickupRange);
        return;
    }

    const int32 SlotIndex = FindSlotForPickUp();

    if (!Cargo->AttachToCarrier(OwnerPawn, HoldPoint))
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Pickup failed: could not attach %s to HoldPoint=%s. Check that the owner calls SetHoldPoint in its constructor."),
            *OwnerPawn->GetName(), *Cargo->GetName(), *GetNameSafe(HoldPoint.Get()));
        return;
    }

    Slots[SlotIndex] = Cargo;
    ApplyActiveSlot(SlotIndex);
}

void USPInventoryComponent::ServerDrop_Implementation()
{
    // 들려 있던 Large는 인원이 모자라게 되면 그 자리에서 떨어진다. 잡기 상태는 OnLargeReleased에서 풀린다.
    if (ASPCargo* Large = GetGrippedLarge())
    {
        Large->RemoveLargeCarrier(GetOwner<APawn>());
        return;
    }

    ASPCargo* Cargo = GetActiveCargo();
    FVector DropLocation;

    // 앞이 막혀 있으면 계속 든다. 벽에 붙어 누른 정상 상황이라 기록하지 않는다.
    if (!Cargo || !FindDropLocation(*Cargo, DropLocation))
    {
        return;
    }

    Cargo->DetachFromCarrier(DropLocation);
    Slots[ActiveSlot] = nullptr;

    // 서버에서는 RepNotify가 자동 호출되지 않는다.
    OnRep_Inventory();
}

void USPInventoryComponent::ServerSelectSlot_Implementation(int32 SlotIndex)
{
    // 양손 화물을 들거나 Large를 잡은 동안에는 전환할 수 없다. 버려야 풀린다.
    if (!Slots.IsValidIndex(SlotIndex)
        || SlotIndex == ActiveSlot
        || IsHoldingTwoHanded()
        || IsGrippingLarge())
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

    // bHidden은 복제되므로 서버에서만 바꾸면 모든 화면에 반영된다.
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
    {
        if (IsValid(Slots[Index]))
        {
            Slots[Index]->SetActorHiddenInGame(Index != ActiveSlot);
        }
    }

    // 서버에서는 RepNotify가 자동 호출되지 않는다.
    OnRep_Inventory();
}

bool USPInventoryComponent::FindDropLocation(
    const ASPCargo& Cargo,
    FVector& OutLocation) const
{
    const AActor* Owner = GetOwner();

    // 운반 중에는 충돌이 꺼져 있으므로 충돌 없는 컴포넌트까지 포함해 크기를 잰다.
    const FBox CargoBounds = Cargo.GetComponentsBoundingBox(true);
    const FVector CargoExtent = CargoBounds.GetExtent();

    float OwnerRadius = 0.0f;
    float OwnerHalfHeight = 0.0f;
    Owner->GetSimpleCollisionCylinder(OwnerRadius, OwnerHalfHeight);

    // 소유자 중심에서 몸 정면으로, 화물이 소유자와 겹치지 않는 거리까지 화물 크기의 상자를 쓸어 본다.
    const FVector Start = Owner->GetActorLocation();
    const float Distance = OwnerRadius + CargoExtent.Size2D() + DropGap;
    const FVector End = Start + Owner->GetActorForwardVector() * Distance;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SPCargoDrop), false, Owner);
    Params.AddIgnoredActor(&Cargo);

    FHitResult Hit;
    const bool bHit = GetWorld()->SweepSingleByChannel(
        Hit,
        Start,
        End,
        FQuat::Identity,
        DropTraceChannel,
        FCollisionShape::MakeBox(CargoExtent),
        Params);

    // 시작부터 겹치면(벽에 붙음, 화물이 소유자보다 큼) 놓을 자리가 없다.
    if (bHit && Hit.bStartPenetrating)
    {
        return false;
    }

    const FVector DropCenter = bHit ? Hit.Location : End;

    // 상자 중심 기준으로 찾았으므로 메시 피벗 위치로 되돌린다.
    OutLocation = DropCenter
        + (Cargo.GetActorLocation() - CargoBounds.GetCenter());
    return true;
}

void USPInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 소유자가 파괴(접속 종료 등)되면 가진 화물이 숨겨지거나 충돌 없는 상태로 남지 않게 제자리에 떨어뜨린다.
    // 레벨 종료 시에는 화물도 함께 정리되므로 처리하지 않는다.
    if (EndPlayReason == EEndPlayReason::Destroyed
        && GetOwner() && GetOwner()->HasAuthority())
    {
        DropAll();
    }

    Super::EndPlay(EndPlayReason);
}

void USPInventoryComponent::DropAll()
{
    if (ASPCargo* Large = GetGrippedLarge())
    {
        Large->RemoveLargeCarrier(GetOwner<APawn>());
    }

    for (TObjectPtr<ASPCargo>& Cargo : Slots)
    {
        if (IsValid(Cargo))
        {
            Cargo->DetachFromCarrier(Cargo->GetActorLocation());
        }

        Cargo = nullptr;
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
        const ASPCargo* Cargo = Slots[Index];
        const FString ItemName = IsValid(Cargo)
            ? UEnum::GetDisplayValueAsText(Cargo->GetWeight()).ToString()
            : TEXT("-");

        Text += FString::Printf(TEXT("%s[%d] %s  "),
            Index == ActiveSlot ? TEXT(">") : TEXT(""),
            Index + 1,
            *ItemName);
    }

    if (IsGrippingLarge())
    {
        Text += TEXT("[Gripping Large]");
    }

    GEngine->AddOnScreenDebugMessage(
        static_cast<uint64>(GetUniqueID()), 3600.0f, FColor::Cyan, Text);
#endif
}
