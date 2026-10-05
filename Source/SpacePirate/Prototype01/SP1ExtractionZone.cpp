// 작성자 : 임진혁
#include "Prototype01/SP1ExtractionZone.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

ASP1ExtractionZone::ASP1ExtractionZone()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    Zone = CreateDefaultSubobject<UBoxComponent>(TEXT("ExtractionBounds"));
    SetRootComponent(Zone);
    Zone->SetBoxExtent(FVector(200,150,120));
    Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Terminal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Terminal"));
    Terminal->SetupAttachment(Zone);
    Terminal->SetCollisionProfileName(TEXT("BlockAll"));
}

bool ASP1ExtractionZone::ContainsPawn(const APawn* Pawn) const
{
    if (!IsValid(Pawn)) return false;
    // 겹침 콜백 순서나 캡슐 반경 대신 출발 시점의 캡슐 중심으로 판정한다.
    const FVector Local = Zone->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation()).GetAbs();
    const FVector Extent = Zone->GetUnscaledBoxExtent();
    return Local.X <= Extent.X && Local.Y <= Extent.Y && Local.Z <= Extent.Z;
}

FVector ASP1ExtractionZone::GetInteractionPoint() const
{
    return GetActorTransform().TransformPosition(InteractionOffset);
}

bool ASP1ExtractionZone::ProtectsPawn(const APawn* Pawn) const
{
    if (!bProtectFromHazards || !IsValid(Pawn)) return false;
    const FVector Local = Zone->GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
    const FVector E = Zone->GetUnscaledBoxExtent();
    return Local.X >= -E.X-FMath::Max(200.f,SafeApproachCentimeters) && Local.X <= E.X
        && FMath::Abs(Local.Y) <= E.Y && FMath::Abs(Local.Z) <= E.Z;
}
