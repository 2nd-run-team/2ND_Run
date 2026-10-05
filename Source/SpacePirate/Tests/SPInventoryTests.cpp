// 맵 에셋을 변경하지 않고 1차 MVP안 06장의 인벤토리 규칙(4칸, 등 가방 1개, 가방 속도·달리기, 내려놓기·던지기)을 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPCharacterMovementComponent.h"
#include "SPInventoryComponent.h"
#include "SPCargo.h"
#include "SPPlayerCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

namespace SPInventoryTests
{
    /** 열린 사용자 맵과 별개인 임시 게임 월드. */
    struct FTestWorld
    {
        UWorld* World;
        FTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            World->GetWorldSettings()->NotifyBeginPlay();
        }
        ~FTestWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    };

    /** BP에서 고르는 종류를 BeginPlay 전에 정한다. 플레이어 뒤쪽(-X)에 두어 앞으로 버린 물건과 겹치지 않게 한다. */
    ASPCargo* SpawnItem(UWorld* World, ESPItemType Type, double Y)
    {
        const FTransform Transform(FVector(-150.0, Y, 0.0));
        ASPCargo* Item = World->SpawnActorDeferred<ASPCargo>(ASPCargo::StaticClass(), Transform);
        FEnumProperty* TypeProperty = FindFProperty<FEnumProperty>(ASPCargo::StaticClass(), TEXT("ItemType"));
        *TypeProperty->ContainerPtrToValuePtr<ESPItemType>(Item) = Type;
        Item->FindComponentByClass<UStaticMeshComponent>()->SetStaticMesh(
            LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Item->FinishSpawning(Transform);
        return Item;
    }

    FName GetAttachPointName(const ASPCargo* Item)
    {
        const USceneComponent* Parent = Item->GetRootComponent()->GetAttachParent();
        return Parent ? Parent->GetFName() : NAME_None;
    }
}

// 4칸이 차면 더 집지 못하고, 손 물건은 현재 칸만 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInventorySlotsTest, "SpacePirate.Inventory.Slots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInventorySlotsTest::RunTest(const FString& Parameters)
{
    using namespace SPInventoryTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPInventoryComponent* Inventory = Player->GetInventory();

    TArray<ASPCargo*> Items = {
        SpawnItem(World, ESPItemType::Keycard, -150.0),
        SpawnItem(World, ESPItemType::SubdualTool, -75.0),
        SpawnItem(World, ESPItemType::SmallLoot, 0.0),
        SpawnItem(World, ESPItemType::SmallLoot, 75.0),
    };
    ASPCargo* Extra = SpawnItem(World, ESPItemType::Keycard, 150.0);

    for (ASPCargo* Item : Items)
    {
        Inventory->PickUp(Item);
    }

    TestEqual(TEXT("Four slots"), Inventory->GetSlots().Num(), 4);
    TestFalse(TEXT("All four picked up"), Items.ContainsByPredicate([](const ASPCargo* Item) { return !Item->IsCarried(); }));
    TestFalse(TEXT("Full inventory rejects a fifth item"), Inventory->CanPickUp(Extra));
    TestTrue(TEXT("Last picked item is in hand"), Inventory->GetHandItem() == Items[3]);
    TestTrue(TEXT("Other hand items are hidden"), Items[0]->IsHidden() && !Items[3]->IsHidden());

    Inventory->ServerSelectSlot(0);
    TestTrue(TEXT("Number key selects slot 1"), Inventory->GetHandItem() == Items[0]);
    TestTrue(TEXT("Previous hand item hidden"), Items[3]->IsHidden() && !Items[0]->IsHidden());

    Inventory->ServerCycleSlot(-1);
    TestTrue(TEXT("Wheel wraps to the last slot"), Inventory->GetHandItem() == Items[3]);
    return true;
}

// 가방은 한 칸을 쓰고 등에 붙으며 하나만 멘다. 가방 칸을 고르면 손은 비고, 가방은 항상 보인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInventoryBagTest, "SpacePirate.Inventory.Bag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInventoryBagTest::RunTest(const FString& Parameters)
{
    using namespace SPInventoryTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPInventoryComponent* Inventory = Player->GetInventory();

    ASPCargo* LootBag = SpawnItem(World, ESPItemType::LootBag, -100.0);
    ASPCargo* DrillBag = SpawnItem(World, ESPItemType::DrillBag, 0.0);
    ASPCargo* Keycard = SpawnItem(World, ESPItemType::Keycard, 100.0);

    Inventory->PickUp(LootBag);
    TestTrue(TEXT("Bag takes a slot"), Inventory->GetSlots().Contains(LootBag));
    TestTrue(TEXT("Bag is worn"), Inventory->HasBag());
    TestEqual(TEXT("Bag attaches to the back"), GetAttachPointName(LootBag), FName(TEXT("BackPoint")));
    TestFalse(TEXT("Bag slot leaves hands empty"), Player->IsCarryingCargo());
    TestFalse(TEXT("Second bag rejected even with free slots"), Inventory->CanPickUp(DrillBag));

    Inventory->PickUp(Keycard);
    TestTrue(TEXT("Tool goes to hand"), Inventory->GetHandItem() == Keycard);
    TestEqual(TEXT("Tool attaches to the hand"), GetAttachPointName(Keycard), FName(TEXT("CargoHoldPoint")));
    TestFalse(TEXT("Bag stays visible while a tool is selected"), LootBag->IsHidden());

    Inventory->ServerSelectSlot(Inventory->GetSlots().IndexOfByKey(LootBag));
    TestFalse(TEXT("Selecting the bag slot empties hands"), Player->IsCarryingCargo());
    TestTrue(TEXT("Tool hidden when bag slot selected"), Keycard->IsHidden());
    TestFalse(TEXT("Bag still visible"), LootBag->IsHidden());
    return true;
}

// 가방을 메면 걷기 속도의 70%이고 달리기 속도 상한이 사라진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInventoryBagSpeedTest, "SpacePirate.Inventory.BagSpeed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInventoryBagSpeedTest::RunTest(const FString& Parameters)
{
    using namespace SPInventoryTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPCharacterMovementComponent* Movement = CastChecked<USPCharacterMovementComponent>(Player->GetCharacterMovement());
    ASPCargo* Bag = SpawnItem(World, ESPItemType::LootBag, 0.0);

    // 낙하 상한은 달리기 속도라 가방의 달리기 제한이 그대로 보인다.
    Movement->SetMovementMode(MOVE_Falling);
    const float FreeFallCap = Movement->GetMaxSpeed();
    TestEqual(TEXT("Without a bag the cap is sprint speed"), FreeFallCap, FMath::Max(Movement->MaxWalkSpeed, Movement->SprintSpeed));

    Player->GetInventory()->PickUp(Bag);
    TestEqual(TEXT("With a bag the cap is 70% of walk speed"), Movement->GetMaxSpeed(), Movement->MaxWalkSpeed * Movement->BagSpeedMultiplier);

    Movement->SetMovementMode(MOVE_Walking);
    TestEqual(TEXT("Walking with a bag is 70%"), Movement->GetMaxSpeed(), Movement->MaxWalkSpeed * Movement->BagSpeedMultiplier);
    return true;
}

// 짧게 누르면 현재 칸 물건을 내려놓고, 길게 누르면 가방만 던진다. 도구는 던지지 않고 내려놓는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInventoryDropThrowTest, "SpacePirate.Inventory.DropThrow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInventoryDropThrowTest::RunTest(const FString& Parameters)
{
    using namespace SPInventoryTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPInventoryComponent* Inventory = Player->GetInventory();

    ASPCargo* Bag = SpawnItem(World, ESPItemType::LootBag, -100.0);
    ASPCargo* Keycard = SpawnItem(World, ESPItemType::Keycard, 100.0);

    Inventory->PickUp(Bag);
    Inventory->ServerDrop();
    TestFalse(TEXT("Dropped bag is free"), Bag->IsCarried());
    TestFalse(TEXT("Bag slot is empty after drop"), Inventory->HasBag());
    TestTrue(TEXT("Dropped bag lands in front"), Bag->GetActorLocation().X > 0.0);

    Inventory->PickUp(Bag);
    Inventory->ServerThrow(1.0f);
    TestFalse(TEXT("Thrown bag is free"), Bag->IsCarried());
    TestTrue(TEXT("Thrown bag flies forward"), Bag->GetVelocity().X > 0.0);

    Inventory->PickUp(Keycard);
    Inventory->ServerThrow(1.0f);
    TestFalse(TEXT("Long press releases a tool"), Keycard->IsCarried());
    TestTrue(TEXT("Tool is placed, not thrown"), Keycard->GetVelocity().IsNearlyZero());
    return true;
}

#endif
