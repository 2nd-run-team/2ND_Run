// 작성자 : 임진혁
#include "Prototype01/SP1TransferCargo.h"
#include "Prototype01/SP1CargoDefinition.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1SessionSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ASP1TransferCargo::ASP1TransferCargo()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CargoMesh"));
    Mesh->SetupAttachment(RootComponent);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    Mesh->SetSimulatePhysics(false);
}

void ASP1TransferCargo::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (Definition)
    {
        Mesh->SetStaticMesh(Definition->Mesh);
        Mesh->SetRelativeScale3D(Definition->MeshScale);
        Mesh->SetRelativeLocation(Definition->MeshOffset);
    }
}

FVector ASP1TransferCargo::GetInteractionPoint() const
{
    return GetActorTransform().TransformPosition(Definition ? Definition->InteractionOffset : FVector::ZeroVector);
}

void ASP1TransferCargo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASP1TransferCargo, bTransferred);
}

void ASP1TransferCargo::SetTransferred(bool bNewTransferred)
{
    if (!HasAuthority() || bTransferred == bNewTransferred) return;
    bTransferred = bNewTransferred;
    OnRep_Transferred(); // Listen Server에도 같은 표현을 정확히 한 번 적용한다.
    ForceNetUpdate();
}

void ASP1TransferCargo::OnRep_Transferred()
{
    if (IsValid(TransferAudio)) TransferAudio->Stop();
    Mesh->SetVisibility(!bTransferred);
    Mesh->SetCollisionEnabled(bTransferred ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
    if (bTransferred && Definition && Definition->CompleteSound && GetNetMode() != NM_DedicatedServer && USP1SessionSubsystem::CanPlayAudio(this))
    {
        TransferAudio=UGameplayStatics::SpawnSoundAtLocation(this,Definition->CompleteSound,GetActorLocation(),FRotator::ZeroRotator,.4f,1,0,Definition->CompleteAttenuation);
        USP1SessionSubsystem::TrackAudio(this,TransferAudio);
    }
}
void ASP1TransferCargo::Tick(float Delta)
{
    Super::Tick(Delta);
    const auto* Round=USP1RoundComponent::Find(this);
    if (IsValid(TransferAudio) && (!Round || !SP1::IsPlaying(Round->State.Phase))) TransferAudio->Stop();
}
void ASP1TransferCargo::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(TransferAudio)) TransferAudio->Stop();
    Super::EndPlay(Reason);
}
