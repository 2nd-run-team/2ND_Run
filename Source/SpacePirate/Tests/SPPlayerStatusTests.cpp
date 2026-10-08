// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 신규 작성
// 변경 내용: 피해·다운 제한·구조 자격·취소·스테이지 초기화·웅크린 자세 복구를 자동 검사한다.
// 작업자: 김세훈 | 2026-10-08 | 다운 캡슐 정렬 회귀 검사
// 변경 내용: 빈손/화물 운반 시 누운 캡슐의 머리 쪽 적중, 메시 중복 회전 방지와 구조 후 직립을 확인한다.

// 임시 월드에서 피해부터 다운, 동료 구조까지 서버 규칙을 검증한다. 실제 맵/BP는 변경하지 않는다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SPCargo.h"
#include "SPCharacterMovementComponent.h"
#include "SPGuardCharacter.h"
#include "SPInteractableComponent.h"
#include "SPInteractorComponent.h"
#include "SPInventoryComponent.h"
#include "SPPlayerCharacter.h"
#include "SPPlayerStatusComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include <limits>

namespace SPPlayerStatusTests
{
    struct FWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);

        FWorld()
        {
            FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
            Context.SetCurrentWorld(World);
            UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
            Context.OwningGameInstance = GameInstance;
            World->SetGameInstance(GameInstance);
            // APawn::ShouldTakeDamage는 서버 GameMode가 있어야 일반 엔진 피해를 허용한다.
            // 작전 실패 규칙과 무관하게 개별 플레이어 상태를 시험하기 위해 기본 모드를 사용한다.
            FURL URL;
            URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
            World->SetGameMode(URL);
            World->InitializeActorsForPlay(URL);
            World->BeginPlay();
            World->GetWorldSettings()->NotifyBeginPlay();
        }

        ~FWorld()
        {
            World->EndPlay(EEndPlayReason::Quit);
            World->DestroyWorld(false);
            GEngine->DestroyWorldContext(World);
        }

        ASPPlayerCharacter* Player(const FVector& Location)
        {
            APlayerController* Controller = World->SpawnActor<APlayerController>();
            Controller->PlayerState = World->SpawnActor<APlayerState>();
            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ASPPlayerCharacter* Pawn = World->SpawnActor<ASPPlayerCharacter>(Location, FRotator::ZeroRotator, Spawn);
            Controller->Possess(Pawn);
            return Pawn;
        }

        ASPCargo* Item(ESPItemType Type, double Y)
        {
            const FTransform Transform(FVector(-450.0, Y, 96.0));
            ASPCargo* Cargo = World->SpawnActorDeferred<ASPCargo>(ASPCargo::StaticClass(), Transform);
            FEnumProperty* Property = FindFProperty<FEnumProperty>(ASPCargo::StaticClass(), TEXT("ItemType"));
            *Property->ContainerPtrToValuePtr<ESPItemType>(Cargo) = Type;
            Cargo->FindComponentByClass<UStaticMeshComponent>()->SetStaticMesh(
                LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
            Cargo->FinishSpawning(Transform);
            return Cargo;
        }

        USPInteractableComponent* WorkTarget(const FVector& Location)
        {
            AActor* Actor = World->SpawnActor<AActor>();
            Actor->SetReplicates(true);
            USceneComponent* Root = NewObject<USceneComponent>(Actor);
            Actor->SetRootComponent(Root);
            Root->RegisterComponent();
            Actor->SetActorLocation(Location);
            USPInteractableComponent* Target = NewObject<USPInteractableComponent>(Actor);
            Target->HoldDuration = 10.0f;
            Target->RegisterComponent();
            return Target;
        }

        AActor* Wall(const FVector& Location)
        {
            AActor* Actor = World->SpawnActor<AActor>();
            UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
            Actor->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(10.0, 200.0, 250.0));
            Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->RegisterComponent();
            Actor->SetActorLocation(Location);
            return Actor;
        }
    };

    void Tick(USPInteractableComponent* Target, float Seconds)
    {
        Target->TickComponent(Seconds, LEVELTICK_All, &Target->PrimaryComponentTick);
    }

    void Down(ASPPlayerCharacter* Player)
    {
        UGameplayStatics::ApplyDamage(Player, Player->GetStatusComponent()->GetMaxHealth(), nullptr, nullptr, nullptr);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayerStatusDamageTest, "SpacePirate.PlayerStatus.DamageAndDownRestrictions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPPlayerStatusDamageTest::RunTest(const FString& Parameters)
{
    using namespace SPPlayerStatusTests;
    FWorld Fixture;
    if (!TestNotNull(TEXT("Damage fixture has an authoritative game mode"), Fixture.World->GetAuthGameMode())) { return false; }
    ASPPlayerCharacter* Player = Fixture.Player(FVector(0.0, 0.0, 96.0));
    USPPlayerStatusComponent* Life = Player->GetStatusComponent();
    if (!TestNotNull(TEXT("Player owns life component"), Life)) { return false; }
    const float MaxHealth = Life->GetMaxHealth();
    TestEqual(TEXT("Spawns at maximum health"), Life->GetHealth(), MaxHealth);

    // 일반 엔진 피해도 컴포넌트로 들어와야 경비/환경 피해를 같은 규칙으로 처리할 수 있다.
    UGameplayStatics::ApplyDamage(Player, MaxHealth * 0.25f, nullptr, nullptr, nullptr);
    TestEqual(TEXT("Engine damage reaches life component"), Life->GetHealth(), MaxHealth * 0.75f);
    Life->ApplyDamage(-20.0f);
    Life->ApplyDamage(0.0f);
    Life->ApplyDamage(std::numeric_limits<float>::quiet_NaN());
    Life->ApplyDamage(std::numeric_limits<float>::infinity());
    TestEqual(TEXT("Invalid damage neither heals nor corrupts health"), Life->GetHealth(), MaxHealth * 0.75f);
    TestEqual(TEXT("Health percentage remains finite"), Life->GetHealthPercent(), 0.75f);

    USPInventoryComponent* Inventory = Player->GetInventory();
    ASPCargo* Bag = Fixture.Item(ESPItemType::LootBag, -100.0);
    ASPCargo* Keycard = Fixture.Item(ESPItemType::Keycard, 0.0);
    ASPCargo* Tool = Fixture.Item(ESPItemType::CCTool, 100.0);
    Inventory->PickUp(Bag);
    Inventory->PickUp(Keycard);
    Inventory->PickUp(Tool);
    USPInteractableComponent* Work = Fixture.WorkTarget(FVector(120.0, 0.0, 96.0));
    Player->GetInteractor()->StartInteract(Work);
    Tick(Work, 1.0f);
    TestTrue(TEXT("Player is working before damage"), Player->GetInteractor()->IsHolding());

    Life->ApplyDamage(MaxHealth * 2.0f);
    TestTrue(TEXT("Lethal damage downs player"), Player->IsDowned());
    TestEqual(TEXT("Overkill clamps health to zero"), Life->GetHealth(), 0.0f);
    TestTrue(TEXT("Down interrupts current action"), !Player->GetInteractor()->IsHolding() && !Work->GetCurrentUser());
    TestTrue(TEXT("Down disables movement"), Player->GetCharacterMovement()->MovementMode == MOVE_None);
    USPCharacterMovementComponent* Movement = CastChecked<USPCharacterMovementComponent>(Player->GetCharacterMovement());
    TestFalse(TEXT("Gravity change cannot reactivate downed player"), Movement->SetGravityMode(ESPGravityMode::ZeroGravity));
    TestTrue(TEXT("Rejected gravity change keeps movement disabled"), Movement->MovementMode == MOVE_None);
    TestFalse(TEXT("Only worn bag is released"), Inventory->HasBag() || Bag->IsCarried());
    TestTrue(TEXT("Keycard remains in its slot"), Keycard->IsCarried() && Inventory->GetSlots().Contains(Keycard));
    TestTrue(TEXT("Tool remains in its slot"), Tool->IsCarried() && Inventory->GetSlots().Contains(Tool));

    Inventory->ServerDrop();
    Inventory->ServerThrow(1.0f);
    Inventory->ServerSelectSlot(Inventory->GetSlots().IndexOfByKey(Keycard));
    Inventory->ServerCycleSlot(1);
    TestTrue(TEXT("Downed inventory requests cannot release or change the held tool"), Inventory->GetActiveItem() == Tool && Tool->IsCarried());
    TestFalse(TEXT("Downed player cannot pick up dropped bag"), Inventory->CanPickUp(Bag));
    Inventory->PickUp(Bag);
    TestFalse(TEXT("Server pickup also rejects downed player"), Bag->IsCarried());

    // 클라이언트 사전 검사와 별개로 서버 RPC를 직접 호출해 상태 검사를 확인한다.
    UFunction* StartRequest = Player->GetInteractor()->FindFunction(TEXT("ServerStartInteract"));
    if (TestNotNull(TEXT("Server interaction request exists"), StartRequest))
    {
        struct FStartRequest { AActor* TargetActor; } Request{Work->GetOwner()};
        Player->GetInteractor()->ProcessEvent(StartRequest, &Request);
        TestNull(TEXT("Downed player's server interaction request is rejected"), Work->GetCurrentUser());
    }

    const FVector DroppedLocation = Bag->GetActorLocation();
    Life->ApplyDamage(MaxHealth);
    TestEqual(TEXT("Further damage keeps health at zero"), Life->GetHealth(), 0.0f);
    TestTrue(TEXT("Repeated damage leaves dropped bag and remaining inventory alone"),
        Bag->GetActorLocation().Equals(DroppedLocation) && Keycard->IsCarried() && Tool->IsCarried());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayerStatusReviveTest, "SpacePirate.PlayerStatus.QualifiedExclusiveRevive",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPPlayerStatusReviveTest::RunTest(const FString& Parameters)
{
    using namespace SPPlayerStatusTests;
    FWorld Fixture;
    ASPPlayerCharacter* Patient = Fixture.Player(FVector(0.0, 0.0, 96.0));
    ASPPlayerCharacter* Rescuer = Fixture.Player(FVector(-150.0, 0.0, 96.0));
    ASPPlayerCharacter* Other = Fixture.Player(FVector(0.0, 150.0, 96.0));
    USPPlayerStatusComponent* Life = Patient->GetStatusComponent();
    USPInteractableComponent* Revive = Patient->GetReviveInteraction();
    if (!TestNotNull(TEXT("Player has revive interaction"), Revive)) { return false; }

    Rescuer->GetInteractor()->StartInteract(Revive);
    TestFalse(TEXT("Active player cannot be revived"), Rescuer->GetInteractor()->IsHolding());
    Down(Patient);
    TestFalse(TEXT("Downed player cannot revive itself"), Revive->CanInteract(Patient));
    ASPGuardCharacter* Guard = Fixture.World->SpawnActor<ASPGuardCharacter>();
    Guard->SetActorLocation(FVector(0.0, -150.0, 96.0));
    TestFalse(TEXT("Guard inheriting player character is not a rescuer"), Revive->CanInteract(Guard));

    Rescuer->SetActorLocation(FVector(-500.0, 0.0, 96.0));
    Rescuer->GetInteractor()->StartInteract(Revive);
    TestFalse(TEXT("Distant rescuer is rejected"), Rescuer->GetInteractor()->IsHolding());
    Rescuer->SetActorLocation(FVector(-150.0, 0.0, 96.0));
    Rescuer->GetInteractor()->StartInteract(Revive);
    TestTrue(TEXT("Active teammate starts rescue"), Rescuer->GetInteractor()->IsHolding());
    Tick(Revive, 2.0f);
    TestEqual(TEXT("Half rescue progress after two seconds"), Revive->GetProgress(), 0.5f);
    Other->GetInteractor()->StartInteract(Revive);
    TestFalse(TEXT("Second rescuer cannot take an occupied rescue"), Other->GetInteractor()->IsHolding());
    TestTrue(TEXT("First rescuer remains owner"), Revive->GetCurrentUser() == Rescuer);
    Tick(Revive, 1.9f);
    TestTrue(TEXT("Patient remains down before four seconds"), Patient->IsDowned());
    Tick(Revive, 0.11f);
    TestFalse(TEXT("Four-second rescue restores active state"), Patient->IsDowned());
    TestEqual(TEXT("Rescue restores thirty percent health"), Life->GetHealth(), Life->GetMaxHealth() * 0.3f);
    TestFalse(TEXT("Completed rescue releases rescuer"), Rescuer->GetInteractor()->IsHolding());
    TestTrue(TEXT("Movement resumes after rescue"), Patient->GetCharacterMovement()->MovementMode != MOVE_None);
    TestFalse(TEXT("Completion cannot be applied twice"), Life->TryRevive(Rescuer));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayerStatusCancelTest, "SpacePirate.PlayerStatus.RescueCancellation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPPlayerStatusCancelTest::RunTest(const FString& Parameters)
{
    using namespace SPPlayerStatusTests;
    FWorld Fixture;
    ASPPlayerCharacter* Patient = Fixture.Player(FVector(0.0, 0.0, 96.0));
    ASPPlayerCharacter* Rescuer = Fixture.Player(FVector(-150.0, 0.0, 96.0));
    USPInteractableComponent* Revive = Patient->GetReviveInteraction();
    Down(Patient);

    Rescuer->GetInteractor()->StartInteract(Revive);
    Tick(Revive, 2.0f);
    Rescuer->GetInteractor()->StopInteract();
    TestEqual(TEXT("Releasing E loses rescue progress"), Revive->GetProgress(), 0.0f);
    TestTrue(TEXT("Releasing E leaves patient down"), Patient->IsDowned());

    Rescuer->GetInteractor()->StartInteract(Revive);
    Tick(Revive, 2.0f);
    TestTrue(TEXT("New rescue needs its own full duration"), Patient->IsDowned());
    Rescuer->SetActorLocation(FVector(-150.0, 50.0, 96.0));
    Tick(Revive, 0.1f);
    TestFalse(TEXT("Moving within interaction range cancels rescue"), Rescuer->GetInteractor()->IsHolding());
    TestEqual(TEXT("Movement loses all progress"), Revive->GetProgress(), 0.0f);

    Rescuer->SetActorLocation(FVector(-150.0, 0.0, 96.0));
    Rescuer->GetInteractor()->StartInteract(Revive);
    Tick(Revive, 1.0f);
    Down(Rescuer);
    Tick(Revive, 0.1f);
    TestNull(TEXT("Rescuer going down releases patient"), Revive->GetCurrentUser());
    TestTrue(TEXT("Rescuer going down cannot finish rescue"), Patient->IsDowned());
    TestEqual(TEXT("Rescuer down loses all progress"), Revive->GetProgress(), 0.0f);
    Rescuer->GetInteractor()->StartInteract(Revive);
    TestFalse(TEXT("Downed teammate cannot start another rescue"), Rescuer->GetInteractor()->IsHolding());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayerStatusCoverResetTest, "SpacePirate.PlayerStatus.CoverAndStageReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPPlayerStatusCoverResetTest::RunTest(const FString& Parameters)
{
    using namespace SPPlayerStatusTests;
    FWorld Fixture;
    ASPPlayerCharacter* Patient = Fixture.Player(FVector(0.0, 0.0, 96.0));
    ASPPlayerCharacter* Rescuer = Fixture.Player(FVector(-150.0, 0.0, 96.0));
    USPInteractableComponent* Revive = Patient->GetReviveInteraction();
    Down(Patient);
    AActor* Wall = Fixture.Wall(FVector(-75.0, 0.0, 150.0));
    Rescuer->GetInteractor()->StartInteract(Revive);
    TestFalse(TEXT("Cannot start rescue through a wall"), Rescuer->GetInteractor()->IsHolding());
    Wall->Destroy();

    Rescuer->GetInteractor()->StartInteract(Revive);
    Tick(Revive, 1.0f);
    TestTrue(TEXT("Removing cover permits rescue"), Rescuer->GetInteractor()->IsHolding());
    Wall = Fixture.Wall(FVector(-75.0, 0.0, 150.0));
    Tick(Revive, 0.1f);
    TestFalse(TEXT("Cover appearing during rescue cancels it"), Rescuer->GetInteractor()->IsHolding());
    TestEqual(TEXT("Cover loses rescue progress"), Revive->GetProgress(), 0.0f);
    Wall->Destroy();

    Rescuer->GetInteractor()->StartInteract(Revive);
    Tick(Revive, 2.0f);
    Patient->GetStatusComponent()->ResetForStage();
    TestFalse(TEXT("New stage clears down state"), Patient->IsDowned());
    TestEqual(TEXT("New stage restores full health"), Patient->GetStatusComponent()->GetHealth(), Patient->GetStatusComponent()->GetMaxHealth());
    TestFalse(TEXT("New stage releases pending rescuer"), Rescuer->GetInteractor()->IsHolding());
    TestEqual(TEXT("New stage discards pending rescue progress"), Revive->GetProgress(), 0.0f);
    TestTrue(TEXT("New stage restores movement"), Patient->GetCharacterMovement()->MovementMode != MOVE_None);

    Down(Patient);
    Rescuer->GetInteractor()->StartInteract(Revive);
    Tick(Revive, 2.0f);
    TestTrue(TEXT("New down does not reuse rescue time from previous stage"), Patient->IsDowned());
    Tick(Revive, 2.01f);
    TestFalse(TEXT("Rescue still works after stage reset"), Patient->IsDowned());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayerStatusCrouchRecoveryTest, "SpacePirate.PlayerStatus.CrouchPoseRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPPlayerStatusCrouchRecoveryTest::RunTest(const FString& Parameters)
{
    using namespace SPPlayerStatusTests;
    FWorld Fixture;
    ASPPlayerCharacter* Player = Fixture.Player(FVector(0.0, 0.0, 400.0));
    ASPPlayerCharacter* Rescuer = Fixture.Player(FVector(-150.0, 0.0, 400.0));
    USPCharacterMovementComponent* Movement = CastChecked<USPCharacterMovementComponent>(Player->GetCharacterMovement());
    UCameraComponent* Camera = Player->FindComponentByClass<UCameraComponent>();
    if (!TestNotNull(TEXT("Player has a camera"), Camera)) { return false; }
    const FTransform StandingMesh = Player->GetMesh()->GetRelativeTransform();
    const FVector StandingCamera = Camera->GetRelativeLocation();
    const float StandingHalfHeight = Player->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

    Movement->SetMovementMode(MOVE_Walking);
    Player->Crouch();
    Movement->Crouch(false);
    if (!TestTrue(TEXT("Fixture enters crouch before being downed"), Player->bIsCrouched)) { return false; }
    Down(Player);
    TestTrue(TEXT("Down pose is also the network smoothing translation target"),
        Player->GetBaseTranslationOffset().Equals(Player->GetMesh()->GetRelativeLocation()));
    TestTrue(TEXT("Down pose is also the network smoothing rotation target"),
        Player->GetBaseRotationOffset().Equals(Player->GetMesh()->GetRelativeRotation().Quaternion()));

    Rescuer->GetInteractor()->StartInteract(Player->GetReviveInteraction());
    Tick(Player->GetReviveInteraction(), 4.01f);
    TestFalse(TEXT("Crouched patient is revived"), Player->IsDowned());
    // 수동 생성한 PC는 LocalPlayer/NetDriver가 없어 원격 PC로 판정된다. 실제 로컬 플레이어처럼
    // ControlledCharacterMove가 실행되도록 GameMode가 사용하는 로컬 지정 함수를 호출한다.
    Player->GetController<APlayerController>()->SetAsLocalPlayerController();
    if (!TestTrue(TEXT("Recovery fixture runs a locally controlled movement frame"), Player->IsLocallyControlled())) { return false; }
    // 구조 시 Ctrl을 놓았다는 입력이 없어도 다음 정상 이동 프레임에서 원래 자세로 복귀해야 한다.
    Movement->TickComponent(0.016f, LEVELTICK_All, &Movement->PrimaryComponentTick);
    TestFalse(TEXT("Rescue clears the old crouch request"), Movement->bWantsToCrouch);
    TestFalse(TEXT("Movement frame returns player to standing"), Player->bIsCrouched);
    TestEqual(TEXT("Standing capsule height restored"), Player->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), StandingHalfHeight);
    TestTrue(TEXT("Standing mesh transform restored"), Player->GetMesh()->GetRelativeTransform().Equals(StandingMesh));
    TestTrue(TEXT("Standing camera height restored"), Camera->GetRelativeLocation().Equals(StandingCamera));
    TestTrue(TEXT("Standing network smoothing translation restored"), Player->GetBaseTranslationOffset().Equals(StandingMesh.GetLocation()));
    TestTrue(TEXT("Standing network smoothing rotation restored"), Player->GetBaseRotationOffset().Equals(StandingMesh.GetRotation()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSPPlayerStatusCapsuleTest, "SpacePirate.PlayerStatus.DownedCapsuleAlignment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSPPlayerStatusCapsuleTest::RunTest(const FString& Parameters)
{
    using namespace SPPlayerStatusTests;
    for (const bool bCarryCargo : { false, true })
    {
        FWorld Fixture;
        ASPPlayerCharacter* Patient = Fixture.Player(FVector(0.0, 0.0, 500.0));
        ASPPlayerCharacter* Rescuer = Fixture.Player(FVector(-150.0, 0.0, 500.0));
        USPCharacterMovementComponent* Movement = CastChecked<USPCharacterMovementComponent>(Patient->GetCharacterMovement());
        ASPCargo* Cargo = bCarryCargo ? Fixture.Item(ESPItemType::SmallLoot, 400.0) : nullptr;
        if (Cargo)
        {
            Patient->GetInventory()->PickUp(Cargo);
            TestTrue(TEXT("Fixture holds cargo before down"), Patient->IsCarryingCargo());
        }
        const FTransform MeshBeforeDown = Patient->GetMesh()->GetRelativeTransform();
        Movement->SetMovementMode(MOVE_Walking);
        Down(Patient);

        TestTrue(TEXT("Downed root capsule is horizontal"), FMath::Abs(Patient->GetActorUpVector().Z) < 0.01f);
        TestTrue(TEXT("Mesh follows root without a second relative rotation"),
            Patient->GetMesh()->GetRelativeTransform().Equals(MeshBeforeDown));
        const UCapsuleComponent* Capsule = Patient->GetCapsuleComponent();
        const FVector HeadSide = Patient->GetActorLocation() + Patient->GetActorUpVector()
            * (Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius() * 0.5f);
        FHitResult Hit;
        Fixture.World->LineTraceSingleByChannel(Hit, HeadSide + FVector(0.0, 0.0, 200.0),
            HeadSide - FVector(0.0, 0.0, 200.0), ECC_Visibility);
        TestTrue(TEXT("Head-side trace outside the old upright capsule hits the downed patient"), Hit.GetActor() == Patient);
        if (Cargo)
        {
            TestTrue(TEXT("Carried cargo remains held and does not block the rescue trace"),
                Cargo->IsCarried() && Patient->IsCarryingCargo());
        }

        Patient->GetStatusComponent()->ResetForStage();
        Down(Patient);
        TestTrue(TEXT("Immediate reset and down cannot accumulate capsule rotation"),
            FMath::Abs(Patient->GetActorUpVector().Z) < 0.01f);

        Rescuer->GetInteractor()->StartInteract(Patient->GetReviveInteraction());
        Tick(Patient->GetReviveInteraction(), 4.01f);
        TestFalse(TEXT("Patient can be revived with or without cargo"), Patient->IsDowned());
        Patient->GetController<APlayerController>()->SetAsLocalPlayerController();
        Movement->TickComponent(0.016f, LEVELTICK_All, &Movement->PrimaryComponentTick);
        TestTrue(TEXT("Recovery returns the capsule upright"), Patient->GetActorUpVector().Z > 0.99f);
    }
    return true;
}

#endif
