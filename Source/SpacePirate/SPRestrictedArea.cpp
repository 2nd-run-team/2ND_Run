#include "SPRestrictedArea.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ASPRestrictedArea::ASPRestrictedArea()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
    SetRootComponent(Volume);
    Volume->SetBoxExtent(FVector(500,500,300));
    Volume->SetCollisionProfileName(TEXT("NoCollision"));
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(Volume);
    Visual->SetCollisionProfileName(TEXT("NoCollision"));
    Visual->SetCanEverAffectNavigation(false);
    AreaName = NSLOCTEXT("Stealth", "RestrictedArea", "Restricted");
}
void ASPRestrictedArea::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPRestrictedArea,bRestrictedEnabled);
    DOREPLIFETIME(ASPRestrictedArea,AreaName);
}
bool ASPRestrictedArea::ContainsLocation(FVector Location) const
{
    if (!bRestrictedEnabled || Location.ContainsNaN()) { return false; }
    // 전달된 지점(플레이어 판정은 액터 중심)이 볼륨 안인지 검사한다. 캡슐 일부의 접촉 판정은 아니다.
    // 바닥 표식 메시나 순찰 경로를 바꿔도 이 볼륨의 구역 규칙은 바뀌지 않는다.
    const FVector Local = Volume->GetComponentTransform().InverseTransformPosition(Location);
    const FVector Extent = Volume->GetUnscaledBoxExtent();
    return FMath::Abs(Local.X)<=Extent.X && FMath::Abs(Local.Y)<=Extent.Y && FMath::Abs(Local.Z)<=Extent.Z;
}
ASPRestrictedArea* ASPRestrictedArea::FindAtLocation(const UObject* WorldContextObject, FVector Location)
{
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject,EGetWorldErrorMode::ReturnNull);
    if (!World) { return nullptr; }
    // 겹친 구역은 경로 이름 순으로 하나를 고른다. 현재는 모두 같은 제한 규칙이며 별도 우선순위는 없다.
    ASPRestrictedArea* Found = nullptr;
    for (TActorIterator<ASPRestrictedArea> It(World); It; ++It)
    {
        if (It->ContainsLocation(Location) && (!Found || It->GetPathName()<Found->GetPathName())) { Found=*It; }
    }
    return Found;
}
void ASPRestrictedArea::SetRestrictedEnabled(bool bEnabled)
{
    if (HasAuthority()) { bRestrictedEnabled=bEnabled; ForceNetUpdate(); }
}
void ASPRestrictedArea::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (bDrawBounds && GetNetMode()!=NM_DedicatedServer)
    { DrawDebugBox(GetWorld(),Volume->GetComponentLocation(),Volume->GetScaledBoxExtent(),Volume->GetComponentQuat(),bRestrictedEnabled?FColor::Orange:FColor::Green,false,-1,0,2); }
#endif
}
