// 맵 에셋을 변경하지 않고 열 수 있는 대상(키카드 보안문, 일반 보관함)을 검증한다. 구현안 2.3·3·5장.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPCargo.h"
#include "SPInteractableComponent.h"
#include "SPInteractorComponent.h"
#include "SPInventoryComponent.h"
#include "SPOpenable.h"
#include "SPPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

namespace SPOpenableTests
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

    ASPCargo* SpawnItem(UWorld* World, ESPItemType Type, const FVector& Location)
    {
        const FTransform Transform(Location);
        ASPCargo* Item = World->SpawnActorDeferred<ASPCargo>(ASPCargo::StaticClass(), Transform);
        FEnumProperty* TypeProperty = FindFProperty<FEnumProperty>(ASPCargo::StaticClass(), TEXT("ItemType"));
        *TypeProperty->ContainerPtrToValuePtr<ESPItemType>(Item) = Type;
        Item->FindComponentByClass<UStaticMeshComponent>()->SetStaticMesh(
            LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Item->FinishSpawning(Transform);
        return Item;
    }

    /** BP에서 정하는 설정을 BeginPlay 전에 넣는다. */
    ASPOpenable* SpawnOpenable(UWorld* World, bool bRequiresKeycard, float HoldSeconds, double ContentsExtent)
    {
        const FTransform Transform(FVector(100.0, 0.0, 0.0));
        ASPOpenable* Openable = World->SpawnActorDeferred<ASPOpenable>(ASPOpenable::StaticClass(), Transform);
        FindFProperty<FBoolProperty>(ASPOpenable::StaticClass(), TEXT("bRequiresHandItem"))
            ->SetPropertyValue_InContainer(Openable, bRequiresKeycard);
        Openable->GetInteractable()->HoldDuration = HoldSeconds;
        Openable->GetInteractable()->BlockedPrompt = FText::FromString(TEXT("키카드 필요"));
        Openable->FindComponentByClass<UBoxComponent>()->SetBoxExtent(FVector(ContentsExtent));
        Openable->FinishSpawning(Transform);
        return Openable;
    }

    void Tick(ASPOpenable* Openable, float Seconds)
    {
        USPInteractableComponent* Interactable = Openable->GetInteractable();
        Interactable->TickComponent(Seconds, LEVELTICK_All, &Interactable->PrimaryComponentTick);
    }
}

// 키카드를 손에 들어야 열리고, 카드는 소모되지 않으며, 열린 뒤에는 거절 안내가 사라진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPOpenableKeycardTest, "SpacePirate.Openable.Keycard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPOpenableKeycardTest::RunTest(const FString& Parameters)
{
    using namespace SPOpenableTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPInventoryComponent* Inventory = Player->GetInventory();
    USPInteractorComponent* Interactor = Player->GetInteractor();

    ASPOpenable* Door = SpawnOpenable(World, true, 0.0f, 0.0);
    Interactor->StartInteract(Door->GetInteractable());
    TestFalse(TEXT("No card keeps the door locked"), Door->IsOpen());

    ASPCargo* Card = SpawnItem(World, ESPItemType::Keycard, FVector(-150.0, 0.0, 0.0));
    Inventory->PickUp(Card);
    const int32 CardSlot = Inventory->GetSlots().IndexOfByKey(Card);
    Inventory->ServerSelectSlot(CardSlot == 0 ? 1 : 0);
    Interactor->StartInteract(Door->GetInteractable());
    TestFalse(TEXT("Card in another slot keeps the door locked"), Door->IsOpen());

    Inventory->ServerSelectSlot(CardSlot);
    Interactor->StartInteract(Door->GetInteractable());
    TestTrue(TEXT("Card in hand opens the door"), Door->IsOpen());
    TestTrue(TEXT("Card is not consumed"), Inventory->FindItemOfType(ESPItemType::Keycard) == Card);
    TestTrue(TEXT("Open door shows no blocked prompt"), Door->GetInteractable()->GetBlockedPrompt(Player).IsEmpty());
    TestFalse(TEXT("Open door cannot be used again"), Door->GetInteractable()->CanInteract(Player));
    TestFalse(TEXT("A door is not a clue by default"), Door->IsClue());
    return true;
}

// 보관함은 정해진 시간을 눌러야 열리고, 안에 놓인 물건만 처음 줍는 순간 범죄가 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPOpenableLockerTest, "SpacePirate.Openable.Locker",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPOpenableLockerTest::RunTest(const FString& Parameters)
{
    using namespace SPOpenableTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>(FVector::ZeroVector, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPInteractorComponent* Interactor = Player->GetInteractor();

    // 보관함이 시작할 때 안의 물건을 표시하므로 물건을 먼저 놓는다.
    ASPCargo* Inside = SpawnItem(World, ESPItemType::SmallLoot, FVector(100.0, 0.0, 0.0));
    ASPCargo* Outside = SpawnItem(World, ESPItemType::SmallLoot, FVector(100.0, 200.0, 0.0));
    ASPOpenable* Locker = SpawnOpenable(World, false, 2.0f, 40.0);

    TestTrue(TEXT("Item inside the locker is marked"),
        Inside->GetInteractable()->CrimeKind == ESPCrimeKind::ContainerTheft && Inside->GetInteractable()->bInstantCrime);
    TestTrue(TEXT("Item outside the locker is not marked"),
        Outside->GetInteractable()->CrimeKind == ESPCrimeKind::None);

    Interactor->StartInteract(Locker->GetInteractable());
    Tick(Locker, 1.0f);
    Interactor->StopInteract();
    TestFalse(TEXT("Releasing early keeps the locker closed"), Locker->IsOpen());

    Interactor->StartInteract(Locker->GetInteractable());
    Tick(Locker, 2.1f);
    TestTrue(TEXT("Holding the full time opens the locker"), Locker->IsOpen());

    Interactor->StartInteract(Inside->GetInteractable());
    TestTrue(TEXT("Item is picked up"), Player->GetInventory()->GetSlots().Contains(Inside));
    TestTrue(TEXT("Mark is cleared after the first pickup"), Inside->GetInteractable()->CrimeKind == ESPCrimeKind::None);
    return true;
}

#endif
