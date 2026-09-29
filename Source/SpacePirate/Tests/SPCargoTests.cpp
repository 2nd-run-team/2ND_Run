// 맵 에셋을 변경하지 않고 Large 화물의 잡기·들기·따라가기·놓침 규칙을 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPCargo.h"
#include "SPCharacterMovementComponent.h"
#include "SPGravityZone.h"
#include "SPInventoryComponent.h"
#include "SPPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

namespace SPCargoTests
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

    /** BP처럼 BeginPlay 전에 무게 등급을 정해야 잡는 지점이 만들어진다. 기본 잡는 지점은 (-100,0,0), (100,0,0). */
    ASPCargo* SpawnLargeCargo(UWorld* World, const FVector& Location)
    {
        const FTransform Transform(Location);
        ASPCargo* Cargo = World->SpawnActorDeferred<ASPCargo>(ASPCargo::StaticClass(), Transform);
        FEnumProperty* Weight = FindFProperty<FEnumProperty>(ASPCargo::StaticClass(), TEXT("Weight"));
        *Weight->ContainerPtrToValuePtr<ESPCargoWeight>(Cargo) = ESPCargoWeight::Large;
        Cargo->FindComponentByClass<UStaticMeshComponent>()->SetStaticMesh(
            LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Cargo->FinishSpawning(Transform);
        return Cargo;
    }
}

// 중력 2인 들기, 3번째 거부, 혼자 당기면 안 움직임, 같이 당기면 이동, 반대로 당기면 상쇄, 멀어지면 놓침, 무중력 1인 들기, 중력 복귀 시 낙하, 버리기 키로 놓기.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLargeCargoTest, "SpacePirate.Cargo.Large",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLargeCargoTest::RunTest(const FString& Parameters)
{
    SPCargoTests::FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPGravityZone* Zone = World->SpawnActor<ASPGravityZone>();
    Zone->FindComponentByClass<UBoxComponent>()->SetBoxExtent(FVector(10000.0));
    Zone->SetGravityMode(ESPGravityMode::Gravity);

    ASPCargo* Cargo = SPCargoTests::SpawnLargeCargo(World, FVector::ZeroVector);
    ASPPlayerCharacter* A = World->SpawnActor<ASPPlayerCharacter>(FVector(-150, 0, 0), FRotator::ZeroRotator);
    ASPPlayerCharacter* B = World->SpawnActor<ASPPlayerCharacter>(FVector(150, 0, 0), FRotator::ZeroRotator);
    ASPPlayerCharacter* C = World->SpawnActor<ASPPlayerCharacter>(FVector(-150, 100, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Spawned actors"), Cargo) || !A || !B || !C) { return false; }
    USPInventoryComponent* InvA = A->GetInventory();
    USPInventoryComponent* InvB = B->GetInventory();

    TestTrue(TEXT("Empty hands can grip Large"), InvA->CanPickUp(Cargo));
    InvA->ServerPickUp(Cargo);
    TestTrue(TEXT("A grips"), InvA->IsGrippingLarge());
    TestFalse(TEXT("One carrier cannot lift in gravity"), Cargo->IsLifted());
    TestFalse(TEXT("Same carrier cannot take a second grip"), InvA->CanPickUp(Cargo));

    const double RestZ = Cargo->GetActorLocation().Z;
    InvB->ServerPickUp(Cargo);
    TestTrue(TEXT("Two carriers lift in gravity"), Cargo->IsLifted());
    TestTrue(TEXT("Lift raises the cargo"), Cargo->GetActorLocation().Z > RestZ);
    TestFalse(TEXT("No free grip point for a third player"), C->GetInventory()->CanPickUp(Cargo));

    const FVector Before = Cargo->GetActorLocation();
    const FVector Step(50, 20, 0);
    A->AddActorWorldOffset(Step);
    Cargo->Tick(0.016f);
    TestTrue(TEXT("One carrier moving alone does not move the cargo"), Cargo->GetActorLocation().Equals(Before, 0.1));

    // 원격 플레이어의 이동처럼 한 프레임 늦게 따라와도 누적으로 맞춰진다.
    B->AddActorWorldOffset(Step);
    Cargo->Tick(0.016f);
    TestTrue(TEXT("Cargo moves once the partner catches up"), Cargo->GetActorLocation().Equals(Before + Step, 0.1));

    A->AddActorWorldOffset(FVector(40, 0, 0));
    B->AddActorWorldOffset(FVector(-40, 0, 0));
    Cargo->Tick(0.016f);
    TestTrue(TEXT("Opposite pulls cancel"), Cargo->GetActorLocation().Equals(Before + Step, 0.1));
    A->AddActorWorldOffset(FVector(-40, 0, 0));
    B->AddActorWorldOffset(FVector(40, 0, 0));

    B->AddActorWorldOffset(FVector(1000, 0, 0));
    Cargo->Tick(0.016f);
    TestFalse(TEXT("Distant carrier loses grip"), InvB->IsGrippingLarge());
    TestFalse(TEXT("Too few carriers drop the cargo"), Cargo->IsLifted());
    TestTrue(TEXT("Remaining carrier keeps grip"), InvA->IsGrippingLarge());

    Zone->SetGravityMode(ESPGravityMode::ZeroGravity);
    Cargo->Tick(0.016f);
    TestTrue(TEXT("One carrier lifts in zero gravity"), Cargo->IsLifted());

    Zone->SetGravityMode(ESPGravityMode::Gravity);
    Cargo->Tick(0.016f);
    TestFalse(TEXT("Gravity return with too few carriers drops the cargo"), Cargo->IsLifted());

    InvA->ServerDrop();
    TestFalse(TEXT("Drop key releases the grip"), InvA->IsGrippingLarge());
    return true;
}

// 무중력에서 한 명이 멀어지는 쪽으로 표류해도 잡는 지점의 줄 길이에서 멈추고 놓치지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPLargeCargoLeashTest, "SpacePirate.Cargo.LargeLeash",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPLargeCargoLeashTest::RunTest(const FString& Parameters)
{
    SPCargoTests::FTestWorld TestWorld;
    UWorld* World = TestWorld.World;

    ASPGravityZone* Zone = World->SpawnActor<ASPGravityZone>();
    Zone->FindComponentByClass<UBoxComponent>()->SetBoxExtent(FVector(10000.0));
    Zone->SetGravityMode(ESPGravityMode::ZeroGravity);

    ASPCargo* Cargo = SPCargoTests::SpawnLargeCargo(World, FVector::ZeroVector);
    ASPPlayerCharacter* A = World->SpawnActor<ASPPlayerCharacter>(FVector(-150, 0, 0), FRotator::ZeroRotator);
    ASPPlayerCharacter* B = World->SpawnActor<ASPPlayerCharacter>(FVector(150, 0, 0), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Spawned actors"), Cargo) || !A || !B) { return false; }

    // Possess하지 않는 테스트이므로 시작 이동 모드를 명시하고, 한 번 돌려 영역의 무중력을 적용한다.
    USPCharacterMovementComponent* Movement = CastChecked<USPCharacterMovementComponent>(A->GetCharacterMovement());
    Movement->bRunPhysicsWithNoController = true;
    Movement->SetMovementMode(MOVE_Falling);
    Movement->TickComponent(0.05f, LEVELTICK_All, &Movement->PrimaryComponentTick);
    if (!TestTrue(TEXT("Carrier in zero gravity"), Movement->IsZeroGravity())) { return false; }

    A->GetInventory()->ServerPickUp(Cargo);
    B->GetInventory()->ServerPickUp(Cargo);
    TestTrue(TEXT("Both grip"), A->GetInventory()->IsGrippingLarge() && B->GetInventory()->IsGrippingLarge());

    Movement->Velocity = FVector(-300, 0, 0);
    for (int32 Step = 0; Step < 30; ++Step)
    {
        Movement->TickComponent(0.05f, LEVELTICK_All, &Movement->PrimaryComponentTick);
        Cargo->Tick(0.05f);
    }

    FVector GripLocation;
    TestTrue(TEXT("A still owns a grip point"), Cargo->GetGripLocationFor(A, GripLocation));
    TestTrue(TEXT("Leash holds A within leash length"),
        FVector::Dist(A->GetActorLocation(), GripLocation) <= Cargo->GetLeashLength() + 1.0);
    TestTrue(TEXT("Leash does not release A"), A->GetInventory()->IsGrippingLarge());
    const FVector Outward = (A->GetActorLocation() - GripLocation).GetSafeNormal();
    TestTrue(TEXT("Outward drift removed"), FVector::DotProduct(Movement->Velocity, Outward) < 1.0);
    TestTrue(TEXT("Cargo stays while only A pulls"), Cargo->GetActorLocation().Size2D() < 1.0);
    return true;
}

#endif
