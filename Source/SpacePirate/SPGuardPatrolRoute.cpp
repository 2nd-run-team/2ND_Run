#include "SPGuardPatrolRoute.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"

ASPGuardPatrolRoute::ASPGuardPatrolRoute()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    RestrictedArea = CreateDefaultSubobject<UBoxComponent>(TEXT("RestrictedArea"));
    SetRootComponent(RestrictedArea);
    RestrictedArea->SetBoxExtent(FVector(530, 880, 300));
    RestrictedArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    RestrictedArea->SetRelativeLocation(FVector::ZeroVector);
    Points = {FVector(350, -630, 0), FVector(-350, -630, 0),
        FVector(-350, 630, 0), FVector(350, 630, 0)};
}

FVector ASPGuardPatrolRoute::GetPatrolLocation(int32 Index) const
{
    return Points.IsValidIndex(Index) ? GetActorTransform().TransformPosition(Points[Index]) : GetActorLocation();
}

bool ASPGuardPatrolRoute::ContainsLocation(FVector Location) const
{
    const FVector Local = RestrictedArea->GetComponentTransform().InverseTransformPosition(Location);
    const FVector Extent = RestrictedArea->GetUnscaledBoxExtent();
    return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y
        && FMath::Abs(Local.Z) <= Extent.Z;
}

void ASPGuardPatrolRoute::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if WITH_EDITOR
    if (GetWorld() && !GetWorld()->IsGameWorld())
    {
        for (int32 Index = 0; Index < Points.Num(); ++Index)
        {
            const FVector Point = GetPatrolLocation(Index) + FVector(0, 0, 10);
            DrawDebugSphere(GetWorld(), Point, 18, 8, FColor::Cyan);
            if (Index + 1 < Points.Num() || bLoop)
            {
                DrawDebugDirectionalArrow(GetWorld(), Point,
                    GetPatrolLocation((Index + 1) % Points.Num()) + FVector(0, 0, 10), 30, FColor::Cyan);
            }
        }
    }
#endif
}
