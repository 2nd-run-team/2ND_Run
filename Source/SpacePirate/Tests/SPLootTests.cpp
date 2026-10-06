// 맵 에셋을 변경하지 않고 전리품 포장(MVP안 11장)을 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPCargo.h"
#include "SPInteractableComponent.h"
#include "SPInteractorComponent.h"
#include "SPLootBundle.h"
#include "SPPlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

namespace SPLootTests
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

    /** BP에서 지정하는 가방 클래스를 BeginPlay 전에 넣는다. 테스트는 기본 화물 클래스로 대신한다. */
    ASPLootBundle* SpawnBundle(UWorld* World, double Y, bool bWithBagClass = true)
    {
        const FTransform Transform(FVector(0.0, Y, 0.0));
        ASPLootBundle* Bundle = World->SpawnActorDeferred<ASPLootBundle>(ASPLootBundle::StaticClass(), Transform);
        if (bWithBagClass)
        {
            FClassProperty* BagClass = FindFProperty<FClassProperty>(ASPLootBundle::StaticClass(), TEXT("LootBagClass"));
            BagClass->SetObjectPropertyValue_InContainer(Bundle, ASPCargo::StaticClass());
        }
        Bundle->FinishSpawning(Transform);
        return Bundle;
    }

    void Tick(ASPLootBundle* Bundle, float Seconds)
    {
        USPInteractableComponent* Interactable = Bundle->GetInteractable();
        Interactable->TickComponent(Seconds, LEVELTICK_All, &Interactable->PrimaryComponentTick);
    }

    int32 CountBags(UWorld* World)
    {
        int32 Count = 0;
        for (TActorIterator<ASPCargo> It(World); It; ++It)
        {
            ++Count;
        }
        return Count;
    }
}

// 2초를 다 누르면 묶음 자리에 가방 하나가 생기고 묶음은 사라진다. 두 명이 동시에 해도 하나, 중간에 떼면 없다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLootPackTest, "SpacePirate.Loot.Pack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLootPackTest::RunTest(const FString& Parameters)
{
    using namespace SPLootTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* A = World->SpawnActor<ASPPlayerCharacter>(FVector(-100.0, 0.0, 0.0), FRotator::ZeroRotator);
    ASPPlayerCharacter* B = World->SpawnActor<ASPPlayerCharacter>(FVector(100.0, 0.0, 0.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Players"), A) || !B) { return false; }

    ASPLootBundle* Bundle = SpawnBundle(World, 0.0);
    TestEqual(TEXT("Packing takes 2 seconds"), Bundle->GetInteractable()->HoldDuration, 2.0f);

    A->GetInteractor()->StartInteract(Bundle->GetInteractable());
    Tick(Bundle, 1.0f);
    B->GetInteractor()->StartInteract(Bundle->GetInteractable());
    TestFalse(TEXT("Second packer is rejected"), B->GetInteractor()->IsHolding());

    Tick(Bundle, 1.1f);
    TestEqual(TEXT("One bundle makes exactly one bag"), CountBags(World), 1);
    TestFalse(TEXT("Bundle is gone after packing"), IsValid(Bundle));

    ASPLootBundle* Second = SpawnBundle(World, 50.0);
    A->GetInteractor()->StartInteract(Second->GetInteractable());
    Tick(Second, 1.0f);
    A->GetInteractor()->StopInteract();
    Tick(Second, 5.0f);
    TestEqual(TEXT("Released packing makes no bag"), CountBags(World), 1);
    TestTrue(TEXT("Bundle stays after release"), IsValid(Second));
    return true;
}

// 가방 클래스를 지정하지 않으면 전리품이 사라지지 않도록 묶음을 남긴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLootMissingBagTest, "SpacePirate.Loot.MissingBagClass",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLootMissingBagTest::RunTest(const FString& Parameters)
{
    using namespace SPLootTests;
    FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>(FVector(-100.0, 0.0, 0.0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }

    ASPLootBundle* Bundle = SpawnBundle(World, 0.0, false);
    Player->GetInteractor()->StartInteract(Bundle->GetInteractable());

    AddExpectedError(TEXT("LootBagClass is empty"), EAutomationExpectedErrorFlags::Contains, 1);
    Tick(Bundle, 2.1f);
    TestEqual(TEXT("No bag without a bag class"), CountBags(World), 0);
    TestTrue(TEXT("Bundle stays so loot is not lost"), IsValid(Bundle));
    return true;
}

#endif
