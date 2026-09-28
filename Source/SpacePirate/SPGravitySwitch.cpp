#include "SPGravitySwitch.h"
#include "SPDebug.h"
#include "SPGravityZone.h"
#include "Components/StaticMeshComponent.h"

ASPGravitySwitch::ASPGravitySwitch()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
    SetRootComponent(ButtonMesh);
    ButtonMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ButtonMesh->SetCollisionObjectType(ECC_WorldDynamic);
    ButtonMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    ButtonMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    ButtonMesh->SetGenerateOverlapEvents(false);
}

void ASPGravitySwitch::BeginPlay()
{
    Super::BeginPlay();
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
#else
    if (HasAuthority() && !IsValid(TargetGravityZone))
    {
        SP_DEBUG_LOG(Warning, TEXT("%s: Assign TargetGravityZone on the level instance."), *GetName());
    }
#endif
}

bool ASPGravitySwitch::TryActivate()
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return HasAuthority() && IsValid(TargetGravityZone) && TargetGravityZone->TryToggleGravity();
#else
    return false;
#endif
}
