// 맵 에셋을 변경하지 않고 특수 금고(직접 해제, 드릴 설치와 자동 작업)를 검증한다. 구현안 4장.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPCargo.h"
#include "SPInteractableComponent.h"
#include "SPInteractorComponent.h"
#include "SPInventoryComponent.h"
#include "SPPlayerCharacter.h"
#include "SPVault.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

namespace SPVaultTests
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

    ASPCargo* SpawnDrillBag(UWorld* World)
    {
        const FTransform Transform(FVector(-150.0, 0.0, 0.0));
        ASPCargo* Bag = World->SpawnActorDeferred<ASPCargo>(ASPCargo::StaticClass(), Transform);
        FEnumProperty* TypeProperty = FindFProperty<FEnumProperty>(ASPCargo::StaticClass(), TEXT("ItemType"));
        *TypeProperty->ContainerPtrToValuePtr<ESPItemType>(Bag) = ESPItemType::DrillBag;
        Bag->FindComponentByClass<UStaticMeshComponent>()->SetStaticMesh(
            LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Bag->FinishSpawning(Transform);
        return Bag;
    }

    void Tick(USPInteractableComponent* Interactable, float Seconds)
    {
        Interactable->TickComponent(Seconds, LEVELTICK_All, &Interactable->PrimaryComponentTick);
    }

    struct FFixture
    {
        FTestWorld TestWorld;
        ASPPlayerCharacter* A = nullptr;
        ASPPlayerCharacter* B = nullptr;
        ASPVault* Vault = nullptr;

        FFixture()
        {
            UWorld* World = TestWorld.World;
            A = World->SpawnActor<ASPPlayerCharacter>(FVector(0.0, -100.0, 0.0), FRotator::ZeroRotator);
            B = World->SpawnActor<ASPPlayerCharacter>(FVector(0.0, 100.0, 0.0), FRotator::ZeroRotator);
            Vault = World->SpawnActor<ASPVault>(FVector(100.0, 0.0, 0.0), FRotator::ZeroRotator);
        }

        /** 금고에 E를 누른다. 서버가 이 사람이 쓸 수 있는 대상(설치 또는 직접 해제)을 고른다. */
        void Press(ASPPlayerCharacter* Player) const
        {
            Player->GetInteractor()->StartInteract(Vault->GetInteractable());
        }
    };
}

// 직접 해제는 손을 떼도 진행률이 남고, 누르는 동안 조작이 잠기며, 누적 30초에 열린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPVaultDirectTest, "SpacePirate.Vault.Direct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPVaultDirectTest::RunTest(const FString& Parameters)
{
    using namespace SPVaultTests;
    FFixture F;
    if (!TestNotNull(TEXT("Players"), F.A) || !F.B || !F.Vault) { return false; }
    USPInteractableComponent* Direct = F.Vault->GetInteractable();

    F.Press(F.A);
    TestTrue(TEXT("Direct unlock locks controls"), F.A->GetInteractor()->IsControlLocked());
    Tick(Direct, 10.0f);
    F.A->GetInteractor()->StopInteract();
    TestTrue(TEXT("Released progress is kept"), FMath::IsNearlyEqual(Direct->GetProgress(), 1.0f / 3.0f, 0.01f));

    F.Press(F.B);
    TestTrue(TEXT("Another player continues the saved progress"), Direct->GetCurrentUser() == F.B);
    Tick(Direct, 20.1f);
    TestTrue(TEXT("Total 30 seconds opens the vault"), F.Vault->IsOpen());
    TestTrue(TEXT("Open vault is a clue"), F.Vault->IsClue());
    return true;
}

// 드릴 가방을 멘 사람은 설치가 골라지고, 설치되면 가방을 소모하며, 직접 해제 진행률만큼 덜 작업한 뒤 연다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPVaultDrillTest, "SpacePirate.Vault.Drill",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPVaultDrillTest::RunTest(const FString& Parameters)
{
    using namespace SPVaultTests;
    FFixture F;
    if (!TestNotNull(TEXT("Players"), F.A) || !F.B || !F.Vault) { return false; }
    USPInteractableComponent* Direct = F.Vault->GetInteractable();
    USPInteractableComponent* Install = F.Vault->GetDrillInstall();
    USPInventoryComponent* Inventory = F.A->GetInventory();
    Inventory->PickUp(SpawnDrillBag(F.TestWorld.World));

    // B가 직접 해제를 10초 해 둔다(진행률 1/3).
    F.Press(F.B);
    TestTrue(TEXT("Player without a bag gets direct unlock"), Direct->GetCurrentUser() == F.B);
    F.Press(F.A);
    TestFalse(TEXT("Only one player works on the vault"), Install->GetCurrentUser() != nullptr);
    Tick(Direct, 10.0f);
    F.B->GetInteractor()->StopInteract();

    F.Press(F.A);
    TestTrue(TEXT("Player with a drill bag gets drill install"), Install->GetCurrentUser() == F.A);
    F.Press(F.B);
    TestFalse(TEXT("Direct unlock is blocked while installing"), Direct->GetCurrentUser() != nullptr);
    Tick(Install, 1.0f);
    F.A->GetInteractor()->StopInteract();
    TestTrue(TEXT("Cancelled install keeps the bag"), Inventory->FindItemOfType(ESPItemType::DrillBag) != nullptr);

    F.Press(F.A);
    Tick(Install, 3.1f);
    TestTrue(TEXT("Installed drill consumes the bag"), Inventory->FindItemOfType(ESPItemType::DrillBag) == nullptr);
    TestTrue(TEXT("Vault is drilling"), F.Vault->IsDrilling());

    F.Press(F.B);
    TestFalse(TEXT("Direct unlock is blocked while drilling"), Direct->GetCurrentUser() != nullptr);
    TestTrue(TEXT("Blocked prompt shows the drill"),
        Direct->GetBlockedPrompt(F.B).ToString().Contains(TEXT("드릴")));

    // 30초 × (1 - 1/3) = 20초
    F.Vault->Tick(19.0f);
    TestFalse(TEXT("Drill works only the remaining part"), F.Vault->IsOpen());
    F.Vault->Tick(1.5f);
    TestTrue(TEXT("Drill opens the vault"), F.Vault->IsOpen());
    TestFalse(TEXT("Drilling ends when open"), F.Vault->IsDrilling());
    return true;
}

#endif
