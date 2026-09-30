// 작업자: 김세훈 | 맵 에셋을 변경하지 않고 영역 판정과 캐릭터 복귀 충돌을 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPGravityWorldSubsystem.h"
#include "SPGravityZone.h"
#include "SPCharacterMovementComponent.h"
#include "SPPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"

namespace SPGravityTests
{
    /** 작업자: 김세훈 | 각 검증이 독립적인 게임 월드/물리 장면을 사용하도록 수명을 관리한다. */
    struct FTestWorld
    {
        UWorld* World;
        /** 작업자: 김세훈 | 실제 BeginPlay 등록 경로도 검증한다. */
        FTestWorld()
        {
            World = UWorld::CreateWorld(EWorldType::Game, false);
            GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
            World->InitializeActorsForPlay(FURL());
            World->BeginPlay();
            World->GetWorldSettings()->NotifyBeginPlay();
        }
        /** 작업자: 김세훈 | 열린 사용자 맵과 별개인 임시 월드만 정리한다. */
        ~FTestWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }
    };

    /** 작업자: 김세훈 | 메시 에셋 없이 바닥/천장 충돌을 구성한다. */
    AActor* AddBlock(UWorld* World, const FVector& Location)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(500.0, 500.0, 10.0));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->SetWorldLocation(Location);
        Box->RegisterComponent();
        return Actor;
    }
}

// 작업자: 김세훈 | 변환된 박스, 겹침 우선순위, 전환 쿨다운, 제거 후 기본값 검증.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGravityZonesTest, "SpacePirate.Gravity.Zones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGravityZonesTest::RunTest(const FString& Parameters)
{
    SPGravityTests::FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    USPGravityWorldSubsystem* Registry = World->GetSubsystem<USPGravityWorldSubsystem>();
    if (!TestNotNull(TEXT("Game world subsystem"), Registry)) { return false; }
    TestTrue(TEXT("Outside defaults to gravity"),
        Registry->GetGravityModeAtLocation(FVector::ZeroVector) == ESPGravityMode::Gravity);
    ASPGravityZone* Zone = World->SpawnActor<ASPGravityZone>();
    Zone->SetActorTransform(FTransform(FRotator(0, 37, 0), FVector(1000, 500, 0), FVector(2, 1, 1)));
    const FVector Inside = Zone->GetActorTransform().TransformPosition(FVector(490, 490, 0));
    const FVector Outside = Zone->GetActorTransform().TransformPosition(FVector(510, 0, 0));
    TestTrue(TEXT("Rotated/scaled interior point"), Zone->ContainsPoint(Inside));
    TestFalse(TEXT("Point beyond transformed boundary"), Zone->ContainsPoint(Outside));
    TestTrue(TEXT("BeginPlay registers gravity zone"), Registry->FindZoneAtLocation(Inside) == Zone);
    TestTrue(TEXT("First switch succeeds"), Zone->TryToggleGravity());
    TestTrue(TEXT("New state read without cached copy"), Registry->GetGravityModeAtLocation(Inside) == ESPGravityMode::ZeroGravity);
    TestFalse(TEXT("Immediate second switch rejected"), Zone->TryToggleGravity());
    World->Tick(LEVELTICK_All, 0.3f);
    TestTrue(TEXT("Switch available after cooldown"), Zone->TryToggleGravity());

    ASPGravityZone* Override = World->SpawnActor<ASPGravityZone>();
    Override->SetActorTransform(Zone->GetActorTransform());
    // 에디터 설정과 같은 UPROPERTY로 높은 우선순위의 겹치는 영역을 구성한다.
    FIntProperty* Priority = FindFProperty<FIntProperty>(ASPGravityZone::StaticClass(), TEXT("Priority"));
    if (!TestNotNull(TEXT("Priority property"), Priority)) { return false; }
    Priority->SetPropertyValue_InContainer(Override, 10);
    Override->SetGravityMode(ESPGravityMode::ZeroGravity);
    TestTrue(TEXT("Higher priority overrides"), Registry->FindZoneAtLocation(Inside) == Override);
    Override->Destroy();
    TestTrue(TEXT("Destroyed override falls back"), Registry->FindZoneAtLocation(Inside) == Zone);
    Zone->Destroy();
    TestTrue(TEXT("Last zone removed returns gravity"), Registry->GetGravityModeAtLocation(Inside) == ESPGravityMode::Gravity);
    return true;
}

// 작업자: 김세훈 | 서버 환경 적용, 관성 유지, 낮은 천장 아래 복귀, 장애물 제거 후 직립 검증.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPGravityRecoveryTest, "SpacePirate.Gravity.Recovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPGravityRecoveryTest::RunTest(const FString& Parameters)
{
    SPGravityTests::FTestWorld TestWorld;
    UWorld* World = TestWorld.World;
    ASPGravityZone* Zone = World->SpawnActor<ASPGravityZone>();
    Zone->FindComponentByClass<UBoxComponent>()->SetBoxExtent(FVector(10000.0));
    Zone->SetGravityMode(ESPGravityMode::ZeroGravity);
    ASPPlayerCharacter* Player = World->SpawnActor<ASPPlayerCharacter>();
    // 네이티브 테스트 캐릭터에는 BP 메시/손 소켓이 없으므로 운반 지점을 캡슐에 연결한다.
    USceneComponent* HoldPoint = Cast<USceneComponent>(Player->GetDefaultSubobjectByName(TEXT("CargoHoldPoint")));
    HoldPoint->AttachToComponent(Player->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    USPCharacterMovementComponent* Movement = CastChecked<USPCharacterMovementComponent>(Player->GetCharacterMovement());
    Movement->bRunPhysicsWithNoController = true;
    // Possess하지 않는 테스트이므로 시작 이동 모드를 명시한다. MOVE_None은 의도적으로 재활성화하지 않는다.
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = FVector(120.0, 40.0, 0.0);
    Movement->TickComponent(0.05f, LEVELTICK_All, &Movement->PrimaryComponentTick);
    TestTrue(TEXT("Environment switches new player to zero gravity"), Movement->IsZeroGravity());
    TestTrue(TEXT("No-input inertia maintained"), Movement->Velocity.Equals(FVector(120, 40, 0), 0.01));
    Zone->SetGravityMode(ESPGravityMode::Gravity);
    Movement->TickComponent(0.016f, LEVELTICK_All, &Movement->PrimaryComponentTick);
    TestTrue(TEXT("Open space returns to regular falling"), Movement->IsFalling());
    TestTrue(TEXT("Gravity resumes"), Movement->Velocity.Z < 0.0);

    SPGravityTests::AddBlock(World, FVector(0, 0, -10));
    AActor* Ceiling = SPGravityTests::AddBlock(World, FVector(0, 0, 100));
    Zone->SetGravityMode(ESPGravityMode::ZeroGravity);
    Movement->SetGravityMode(ESPGravityMode::ZeroGravity);
    Player->SetActorLocationAndRotation(FVector(0, 0, 40), FRotator(89, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
    Movement->Velocity = FVector::ZeroVector;
    Movement->SetZeroGravityInput(FVector(1, 0, 1));
    Zone->SetGravityMode(ESPGravityMode::Gravity);
    Movement->TickComponent(0.016f, LEVELTICK_All, &Movement->PrimaryComponentTick);
    TestTrue(TEXT("Ceiling prevents standing but retains recovery"), Movement->IsGravityRecovery());
    TestTrue(TEXT("Failed lift reverted"), Player->GetActorLocation().Z <= 40.01);
    TestTrue(TEXT("Gravity still acts; upward thrust ignored"), Movement->Velocity.Z < 0.0);
    TestTrue(TEXT("Horizontal escape input works"), Movement->Velocity.Size2D() > 0.0);
    Ceiling->Destroy();
    Movement->SetZeroGravityInput(FVector::ZeroVector);
    Movement->TickComponent(0.016f, LEVELTICK_All, &Movement->PrimaryComponentTick);
    TestFalse(TEXT("Recovery ends after headroom restored"), Movement->IsGravityRecovery());
    TestTrue(TEXT("Capsule upright"), Player->GetActorUpVector().Equals(FVector::UpVector, 0.01));
    TestTrue(TEXT("Standing capsule stays above floor"), Player->GetActorLocation().Z
        >= Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 0.1);
    return true;
}

#endif
