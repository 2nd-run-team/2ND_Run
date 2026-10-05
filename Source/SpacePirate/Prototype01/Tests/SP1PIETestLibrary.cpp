// 작성자 : 임진혁
#include "Prototype01/Tests/SP1PIETestLibrary.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "SPCargo.h"
#include "SPInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"

void USP1PIETestLibrary::NextTick(USP1InteractionComponent* Interaction, FName Command)
{
#if WITH_EDITOR
    if (!IsValid(Interaction) || !Interaction->GetWorld() || Interaction->GetWorld()->WorldType != EWorldType::PIE) return;
    if (Command != TEXT("ToggleReady") && Command != TEXT("RequestStart") && Command != TEXT("RequestRestart")
        && Command != TEXT("BeginHold") && Command != TEXT("EndHold") && Command != TEXT("RequestPing")) return;
    TWeakObjectPtr<USP1InteractionComponent> Weak = Interaction;
    Interaction->GetWorld()->GetTimerManager().SetTimerForNextTick([Weak, Command]()
    {
        if (auto* Input = Weak.Get())
        {
            if (Command == TEXT("ToggleReady")) Input->ToggleReady();
            else if (Command == TEXT("RequestStart")) Input->RequestStart();
            else if (Command == TEXT("RequestRestart")) Input->RequestRestart();
            else if (Command == TEXT("BeginHold")) Input->BeginHold();
            else if (Command == TEXT("EndHold")) Input->EndHold();
            else if (Command == TEXT("RequestPing")) Input->RequestPing();
        }
    });
#endif
}

void USP1PIETestLibrary::ConfigureTimers(USP1RoundComponent* Round, float MissionSeconds, float DepartureSeconds)
{
#if WITH_EDITOR
    // PIE ActorComponent의 set_editor_property는 Construction Script를 다시 실행할 수 있다.
    // 테스트 시간만 변경하고 컴포넌트/RunId/원장/에셋은 재생성하지 않는다.
    if (!IsValid(Round) || !Round->GetWorld() || Round->GetWorld()->WorldType != EWorldType::PIE || !Round->GetOwner()->HasAuthority()) return;
    Round->MissionSeconds = FMath::Clamp(MissionSeconds,1.0f,3600.0f);
    Round->bUseSettingsAssets = false;
    Round->DepartureSeconds = FMath::Clamp(DepartureSeconds,0.1f,120.0f);
#endif
}

void USP1PIETestLibrary::SetRemainingSeconds(USP1RoundComponent* Round, float Seconds)
{
#if WITH_EDITOR
    if (!IsValid(Round) || !Round->GetWorld() || Round->GetWorld()->WorldType != EWorldType::PIE || !Round->GetOwner()->HasAuthority()) return;
    Round->State.MissionDeadline = Round->Now() + FMath::Max(0.0f, Seconds);
    Round->GetOwner()->ForceNetUpdate();
#endif
}

void USP1PIETestLibrary::WoundNextTick(AActor* Target, float Amount)
{
#if WITH_EDITOR
    if (!IsValid(Target) || !Target->HasAuthority() || Target->GetWorld()->WorldType != EWorldType::PIE) return;
    TWeakObjectPtr<AActor> Weak = Target;
    Target->GetWorld()->GetTimerManager().SetTimerForNextTick([Weak,Amount]()
    {
        if (AActor* Actor = Weak.Get()) if (auto* Life = Actor->FindComponentByClass<USP1SurvivalComponent>())
            Life->QueueWound(Amount,TEXT("PIETestImpact"));
    });
#endif
}
ASPCargo* USP1PIETestLibrary::SpawnCarryFixture(AActor* Context, FVector Position)
{
#if WITH_EDITOR
    if (!IsValid(Context) || !Context->HasAuthority() || Context->GetWorld()->WorldType != EWorldType::PIE) return nullptr;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ASPCargo* Cargo = Context->GetWorld()->SpawnActor<ASPCargo>(Position,FRotator::ZeroRotator,Params);
    if (Cargo)
    {
        auto* Mesh = Cargo->FindComponentByClass<UStaticMeshComponent>();
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
        Mesh->SetRelativeScale3D(FVector(0.15));
        Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
        Cargo->SetReplicateMovement(true); Cargo->ForceNetUpdate();
    }
    return Cargo;
#else
    return nullptr;
#endif
}
void USP1PIETestLibrary::PickupNextTick(APawn* Pawn, ASPCargo* Cargo)
{
#if WITH_EDITOR
    if (!IsValid(Pawn) || Pawn->GetWorld()->WorldType != EWorldType::PIE) return;
    TWeakObjectPtr<APawn> WeakPawn=Pawn; TWeakObjectPtr<ASPCargo> WeakCargo=Cargo;
    Pawn->GetWorld()->GetTimerManager().SetTimerForNextTick([WeakPawn,WeakCargo]()
    { if (APawn* Owner=WeakPawn.Get()) if (auto* Inventory=Owner->FindComponentByClass<USPInventoryComponent>()) Inventory->ServerPickUp(WeakCargo.Get()); });
#endif
}

void USP1PIETestLibrary::AlignMaintenanceDeadlineToAttempt(USP1InteractionComponent* Interaction)
{
#if WITH_EDITOR
    if (!IsValid(Interaction) || Interaction->GetWorld()->WorldType != EWorldType::PIE || !Interaction->GetOwner()->HasAuthority() || !Interaction->Attempt.bActive) return;
    if (auto* Round = USP1RoundComponent::Find(Interaction); Round && Round->CanUseMaintenance())
    {
        Round->State.Maintenance.Deadline = Interaction->Attempt.StartedAt + Interaction->Attempt.Duration;
        Round->GetOwner()->ForceNetUpdate();
    }
#endif
}
void USP1PIETestLibrary::AlignMissionDeadlineToDeparture(USP1RoundComponent* Round)
{
#if WITH_EDITOR
    if (!IsValid(Round) || Round->GetWorld()->WorldType != EWorldType::PIE || !Round->GetOwner()->HasAuthority() || Round->State.Phase != ESP1Phase::ExtractionCountdown) return;
    Round->State.MissionDeadline = Round->State.ExtractionDeadline;
    Round->GetOwner()->ForceNetUpdate();
#endif
}
