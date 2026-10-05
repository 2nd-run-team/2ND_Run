// 작성자 : 임진혁
#include "Prototype01/SP1CarRegion.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
ASP1CarRegion::ASP1CarRegion()
{
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->SetBoxExtent(FVector(900, 480, 300));
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
bool ASP1CarRegion::ContainsPoint(FVector Point) const
{
    const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(Point);
    const FVector Extent = Bounds->GetUnscaledBoxExtent();
    return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
ASP1CarRegion* ASP1CarRegion::FindAt(const UObject* Context, FVector Point)
{
    if (Context && Context->GetWorld())
        for (TActorIterator<ASP1CarRegion> It(Context->GetWorld()); It; ++It)
            if (It->ContainsPoint(Point)) return *It;
    return nullptr;
}
