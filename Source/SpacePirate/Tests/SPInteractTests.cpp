// 맵 에셋을 변경하지 않고 E 길게 누르기의 시작·진행·취소·완료 규칙을 검증한다. 기획: Docs/E_길게누르기_기획안_2026-10-05.md
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPCargo.h"
#include "SPInteractableComponent.h"
#include "SPInventoryComponent.h"
#include "SPInteractorComponent.h"
#include "SPPlayerCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

namespace SPInteractTests
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

    /** 원점에 놓인 대상. 완료 횟수를 Completions에 센다. */
    struct FTarget
    {
        AActor* Actor = nullptr;
        USPInteractableComponent* Interactable = nullptr;
        int32 Completions = 0;

        FTarget(UWorld* World, float Duration)
        {
            Actor = World->SpawnActor<AActor>();
            USceneComponent* Root = NewObject<USceneComponent>(Actor);
            Actor->SetRootComponent(Root);
            Root->RegisterComponent();

            Interactable = NewObject<USPInteractableComponent>(Actor);
            Interactable->HoldDuration = Duration;
            Interactable->RegisterComponent();
            Interactable->OnCompletedNative.AddLambda([this](APawn*) { ++Completions; });
        }

        void Tick(float Seconds) const
        {
            Interactable->TickComponent(Seconds, LEVELTICK_All, &Interactable->PrimaryComponentTick);
        }
    };

    ASPPlayerCharacter* SpawnPlayer(UWorld* World, double X)
    {
        return World->SpawnActor<ASPPlayerCharacter>(FVector(X, 0.0, 0.0), FRotator::ZeroRotator);
    }
}

// 짧게 누르는 대상은 즉시 완료되고, 길게 누르는 대상은 시간이 다 차야 한 번만 완료된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInteractCompleteTest, "SpacePirate.Interact.Complete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInteractCompleteTest::RunTest(const FString& Parameters)
{
    using namespace SPInteractTests;
    FTestWorld TestWorld;
    ASPPlayerCharacter* Player = SpawnPlayer(TestWorld.World, -100.0);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPInteractorComponent* Interactor = Player->GetInteractor();

    FTarget Instant(TestWorld.World, 0.0f);
    Interactor->StartInteract(Instant.Interactable);
    TestEqual(TEXT("Instant target completes on press"), Instant.Completions, 1);
    TestFalse(TEXT("Instant target is not held"), Interactor->IsHolding());

    FTarget Hold(TestWorld.World, 2.0f);
    Interactor->StartInteract(Hold.Interactable);
    TestTrue(TEXT("Hold starts"), Interactor->IsHolding());

    Hold.Tick(1.0f);
    TestEqual(TEXT("Half way"), Player->GetHoldProgress(), 0.5f);
    TestEqual(TEXT("Not completed yet"), Hold.Completions, 0);

    Hold.Tick(1.1f);
    TestEqual(TEXT("Completes when time is up"), Hold.Completions, 1);
    TestFalse(TEXT("Hold ends on completion"), Interactor->IsHolding());
    TestEqual(TEXT("Progress resets after completion"), Hold.Interactable->GetProgress(), 0.0f);

    Hold.Tick(5.0f);
    TestEqual(TEXT("Completes only once"), Hold.Completions, 1);
    return true;
}

// 손을 떼면 진행 취소, 다른 사람이 쓰는 중이면 시작하지 못하고, 진행률 보존 대상은 이어서 완료된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInteractCancelTest, "SpacePirate.Interact.Cancel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInteractCancelTest::RunTest(const FString& Parameters)
{
    using namespace SPInteractTests;
    FTestWorld TestWorld;
    ASPPlayerCharacter* A = SpawnPlayer(TestWorld.World, -100.0);
    ASPPlayerCharacter* B = SpawnPlayer(TestWorld.World, 100.0);
    if (!TestNotNull(TEXT("Players"), A) || !B) { return false; }

    FTarget Target(TestWorld.World, 2.0f);
    A->GetInteractor()->StartInteract(Target.Interactable);
    Target.Tick(1.0f);

    B->GetInteractor()->StartInteract(Target.Interactable);
    TestFalse(TEXT("Second player is rejected while in use"), B->GetInteractor()->IsHolding());
    TestTrue(TEXT("First player keeps holding"), A->GetInteractor()->IsHolding());

    A->GetInteractor()->StopInteract();
    TestFalse(TEXT("Release cancels"), A->GetInteractor()->IsHolding());
    TestEqual(TEXT("Cancel resets progress"), Target.Interactable->GetProgress(), 0.0f);
    Target.Tick(5.0f);
    TestEqual(TEXT("Cancelled hold never completes"), Target.Completions, 0);

    Target.Interactable->bKeepProgress = true;
    B->GetInteractor()->StartInteract(Target.Interactable);
    Target.Tick(1.0f);
    B->GetInteractor()->StopInteract();
    TestEqual(TEXT("Kept progress stays after release"), Target.Interactable->GetProgress(), 0.5f);

    A->GetInteractor()->StartInteract(Target.Interactable);
    Target.Tick(1.1f);
    TestEqual(TEXT("Anyone can continue kept progress"), Target.Completions, 1);
    return true;
}

// 상호작용 거리 안에서는 움직여도 유지되고, 조작자나 대상이 거리 밖으로 벗어나면 취소된다. 조작 잠금도 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInteractDistanceTest, "SpacePirate.Interact.Distance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInteractDistanceTest::RunTest(const FString& Parameters)
{
    using namespace SPInteractTests;
    FTestWorld TestWorld;
    ASPPlayerCharacter* Player = SpawnPlayer(TestWorld.World, -100.0);
    if (!TestNotNull(TEXT("Player"), Player)) { return false; }
    USPInteractorComponent* Interactor = Player->GetInteractor();

    FTarget Far(TestWorld.World, 2.0f);
    Far.Actor->SetActorLocation(FVector(1000.0, 0.0, 0.0));
    Interactor->StartInteract(Far.Interactable);
    TestFalse(TEXT("Too far to start"), Interactor->IsHolding());

    FTarget Target(TestWorld.World, 2.0f);
    Target.Interactable->bLockControls = true;
    Interactor->StartInteract(Target.Interactable);
    TestTrue(TEXT("Lock target locks controls"), Interactor->IsControlLocked());

    Player->SetActorLocation(FVector(-200.0, 50.0, 0.0));
    Target.Tick(0.5f);
    TestTrue(TEXT("Moving within range keeps holding"), Interactor->IsHolding());

    Player->SetActorLocation(FVector(-400.0, 0.0, 0.0));
    Target.Tick(0.1f);
    TestFalse(TEXT("Leaving range cancels"), Interactor->IsHolding());
    TestFalse(TEXT("Controls unlock after cancel"), Interactor->IsControlLocked());

    Player->SetActorLocation(FVector(-100.0, 0.0, 0.0));
    Interactor->StopInteract();
    Interactor->StartInteract(Target.Interactable);
    Target.Actor->SetActorLocation(FVector(400.0, 0.0, 0.0));
    Target.Tick(0.1f);
    TestFalse(TEXT("Target moving away cancels"), Interactor->IsHolding());
    TestEqual(TEXT("No completion after cancels"), Target.Completions, 0);
    return true;
}

// 화물 줍기도 짧게 누르는 상호작용으로 처리된다. 인벤토리가 차거나 이미 누가 든 화물은 줍지 못한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPInteractCargoPickUpTest, "SpacePirate.Interact.CargoPickUp",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPInteractCargoPickUpTest::RunTest(const FString& Parameters)
{
    using namespace SPInteractTests;
    FTestWorld TestWorld;
    ASPPlayerCharacter* A = SpawnPlayer(TestWorld.World, -100.0);
    ASPPlayerCharacter* B = SpawnPlayer(TestWorld.World, 100.0);
    if (!TestNotNull(TEXT("Players"), A) || !B) { return false; }

    TArray<ASPCargo*> Items;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        Items.Add(TestWorld.World->SpawnActor<ASPCargo>(FVector(0.0, 60.0 * Index - 120.0, 0.0), FRotator::ZeroRotator));
    }
    if (!TestNotNull(TEXT("Cargo"), Items[0])) { return false; }

    USPInteractableComponent* First = Items[0]->GetInteractable();
    TestTrue(TEXT("Cargo pickup is a short press"), First && First->IsInstant());

    A->GetInteractor()->StartInteract(First);
    TestTrue(TEXT("Pressing E on cargo picks it up"), Items[0]->IsCarried());
    TestTrue(TEXT("Picked cargo is in the inventory"), A->GetInventory()->GetSlots().Contains(Items[0]));
    TestFalse(TEXT("Short press does not start a hold"), A->GetInteractor()->IsHolding());

    TestFalse(TEXT("Carried cargo cannot be taken by another player"), First->CanInteract(B));
    B->GetInteractor()->StartInteract(First);
    TestFalse(TEXT("Another player's request is rejected"), B->GetInventory()->GetSlots().Contains(Items[0]));

    for (int32 Index = 1; Index < 4; ++Index)
    {
        A->GetInteractor()->StartInteract(Items[Index]->GetInteractable());
    }
    TestFalse(TEXT("Full inventory blocks the fifth pickup"), Items[4]->GetInteractable()->CanInteract(A));
    A->GetInteractor()->StartInteract(Items[4]->GetInteractable());
    TestFalse(TEXT("Fifth cargo stays on the floor"), Items[4]->IsCarried());
    return true;
}

#endif
