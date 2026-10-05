// 작성자 : 임진혁
#include "Prototype01/SP1MaintenanceStation.h"
#include "Components/StaticMeshComponent.h"
ASP1MaintenanceStation::ASP1MaintenanceStation()
{
    bReplicates = true; bAlwaysRelevant = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Terminal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Terminal"));
    Terminal->SetupAttachment(RootComponent);
    Terminal->SetCollisionProfileName(TEXT("BlockAll"));
    ReviveOffsets = {FVector(-300,-100,35),FVector(-300,100,35),FVector(-500,-100,35),FVector(-500,100,35)};
}
FVector ASP1MaintenanceStation::GetInteractionPoint() const
{ return GetActorTransform().TransformPosition(InteractionOffset); }
