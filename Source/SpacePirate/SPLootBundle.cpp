#include "SPLootBundle.h"

#include "SPCargo.h"
#include "SPDebug.h"
#include "SPInteractableComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ASPLootBundle::ASPLootBundle()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    BundleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BundleMesh"));
    SetRootComponent(BundleMesh);

    Interactable = CreateDefaultSubobject<USPInteractableComponent>(TEXT("Interactable"));
    Interactable->HoldDuration = 2.0f;
    // 포장하는 동안만 범죄로 등록한다. 완성된 가방의 줍기·운반은 이 등록을 이어받지 않는다.
    Interactable->CrimeKind = ESPCrimeKind::LootPacking;
    Interactable->Prompt = NSLOCTEXT("SpacePirate", "PackPrompt", "포장");
}

void ASPLootBundle::BeginPlay()
{
    Super::BeginPlay();

    // 완료 이벤트는 서버에서만 호출된다.
    Interactable->OnCompletedNative.AddUObject(this, &ASPLootBundle::HandlePacked);
}

void ASPLootBundle::HandlePacked(APawn* User)
{
    // 가방을 못 만들면 묶음을 남겨 전리품이 사라지지 않게 한다.
    if (!LootBagClass)
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Packing produced no bag: LootBagClass is empty. Set it to BP_SPLootBag in the bundle Blueprint."),
            *GetName());
        return;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    if (!GetWorld()->SpawnActor<ASPCargo>(LootBagClass, GetActorTransform(), Params))
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Packing failed to spawn %s."), *GetName(), *GetNameSafe(LootBagClass.Get()));
        return;
    }

    Destroy();
}
