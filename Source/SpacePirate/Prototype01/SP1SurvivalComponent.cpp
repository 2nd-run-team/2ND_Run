// 작성자 : 임진혁
#include "Prototype01/SP1SurvivalComponent.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "SPInventoryComponent.h"
#include "SPCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

USP1SurvivalComponent::USP1SurvivalComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
}
void USP1SurvivalComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority()) GetOwner()->OnTakeAnyDamage.AddDynamic(this, &ThisClass::TookDamage);
}
void USP1SurvivalComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USP1SurvivalComponent, Wounds);
    DOREPLIFETIME(USP1SurvivalComponent, ServerStamina);
    DOREPLIFETIME(USP1SurvivalComponent, bDead);
    DOREPLIFETIME(USP1SurvivalComponent, DeathCount);
    DOREPLIFETIME(USP1SurvivalComponent, DropBatches);
    DOREPLIFETIME(USP1SurvivalComponent, SpectateTarget);
}
float USP1SurvivalComponent::GetStamina() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return FMath::Clamp(Pawn && Pawn->IsLocallyControlled() ? MovementState.Current : ServerStamina.Current, 0.0f, GetMaximum());
}
bool USP1SurvivalComponent::CanSprint() const { return !IsDead() && SP1::CanSprint(MovementState); }
bool USP1SurvivalComponent::IsExhausted() const { return MovementState.bExhausted; }
bool USP1SurvivalComponent::IsSimulationEnabled() const
{
    const auto* Round = USP1RoundComponent::Find(this);
    return !IsDead() && Round && SP1::IsPlaying(Round->State.Phase);
}
void USP1SurvivalComponent::SimulateMovement(float Delta, bool bEligible, bool bSprintKey)
{
    if (!IsSimulationEnabled()) return;
    SP1::StepStamina(MovementState, Wounds, FMath::Max(0.0f, Delta), bEligible, bSprintKey, SprintDrain, RecoveryDelay, RecoveryRate);
    if (GetOwner()->HasAuthority()) ServerStamina = MovementState;
}
void USP1SurvivalComponent::ReconcileMovement(const FSP1StaminaState& Authoritative, float ServerWounds)
{
    // 같은 Pawn의 상처는 줄어들지 않는다. 늦은 이동 응답이 최신 피해 복제를 되돌리지 못한다.
    LastAuthoritativeWounds = FMath::Max3(LastAuthoritativeWounds, Wounds, ServerWounds);
    Wounds = LastAuthoritativeWounds;
    MovementState = Authoritative;
    MovementState.Current = FMath::Clamp(MovementState.Current, 0.0f, GetMaximum());
}
void USP1SurvivalComponent::QueueWound(float Amount, FName HazardId)
{
    if (!GetOwner()->HasAuthority() || IsDead() || !FMath::IsFinite(Amount) || Amount <= 0) return;
    if (auto* Round = USP1RoundComponent::Find(this)) Round->QueueDamage(this, Amount, HazardId);
}
void USP1SurvivalComponent::TookDamage(AActor*, float Damage, const UDamageType*, AController*, AActor* Causer)
{
    QueueWound(Damage, Causer ? Causer->GetFName() : FName(TEXT("WorldDamage")));
}
void USP1SurvivalComponent::ApplyWound(float Amount, FName HazardId)
{
    if (!GetOwner()->HasAuthority() || IsDead()) return;
    if (const auto* Round = USP1RoundComponent::Find(this); Round && Round->IsHazardProtected(Cast<APawn>(GetOwner()))) return;
    Wounds = FMath::Clamp(Wounds + Amount, 0.0f, 100.0f);
    MovementState.Current = FMath::Min(MovementState.Current, GetMaximum());
    ServerStamina = MovementState;
    auto* Round = USP1RoundComponent::Find(this);
    auto* Hold = GetOwner()->FindComponentByClass<USP1InteractionComponent>();
    if (Round) Round->LogEvent(TEXT("Wounded"), Hold, HazardId, FString::Printf(TEXT("W=%.1f S=%.1f"), Wounds, MovementState.Current), FMath::RoundToInt(Amount));
    // 비치명적 피해는 확보를 취소하지 않는다. 사망 전이와 소지품 해제는 서버에서 딱 한 번만 수행한다.
    if (Wounds >= 100)
    {
        bDead = true; ++DeathCount;
        if (Hold) Hold->CancelOnServer(TEXT("Dead"));
        if (auto* Inventory = GetOwner()->FindComponentByClass<USPInventoryComponent>())
        { Inventory->ReleaseAllForDeath(); ++DropBatches; }
        ApplyDeathPresentation();
        if (Round) Round->LogEvent(TEXT("Died"), Hold, HazardId, TEXT("Wounds100; inventory released once"));
    }
    GetOwner()->ForceNetUpdate();
}
void USP1SurvivalComponent::OnRep_Life()
{
    // 속성 복제보다 이동 응답이 먼저 도착한 경우도 최대 용량을 되돌리지 않는다.
    LastAuthoritativeWounds = FMath::Max(LastAuthoritativeWounds, Wounds);
    Wounds = LastAuthoritativeWounds;
    MovementState.Current = FMath::Min(MovementState.Current, GetMaximum());
    if (IsDead()) ApplyDeathPresentation();
}
void USP1SurvivalComponent::ApplyDeathPresentation()
{
    if (bLocalDeathApplied) return;
    auto* Pawn = Cast<ACharacter>(GetOwner());
    if (!Pawn) return;
    bLocalDeathApplied = true;
    if (auto* Hold = Pawn->FindComponentByClass<USP1InteractionComponent>()) Hold->EndHold();
    Pawn->StopJumping();
    Pawn->GetCharacterMovement()->DisableMovement();
    Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pawn->SetActorHiddenInGame(true);
    if (auto* PC = Cast<APlayerController>(Pawn->GetController()))
    { PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); }
}
void USP1SurvivalComponent::UpdateSpectator()
{
    if (!GetOwner()->HasAuthority() || !IsDead()) return;
    APawn* Candidate = nullptr;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        const auto* Life = Pawn ? Pawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
        if (Pawn && Pawn != GetOwner() && Life && !Life->IsDead() && PC->PlayerState && !PC->PlayerState->IsOnlyASpectator())
        { Candidate = Pawn; break; }
    }
    SpectateTarget = Candidate;
}
void USP1SurvivalComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    auto* Pawn = Cast<APawn>(GetOwner());
    // 죽은 Pawn의 관전 입력 제한은 Controller에 남으므로 새 소유 Pawn에서 한 번 해제한다.
    if (Pawn && Pawn->IsLocallyControlled() && !IsDead() && !bLocalControlInitialized)
        if (auto* PC = Cast<APlayerController>(Pawn->GetController()))
        {
            PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput(); PC->SetViewTarget(Pawn);
            bLocalControlInitialized = true;
        }
    if (!IsDead() || !Pawn || !Pawn->IsLocallyControlled()) return;
    ApplyDeathPresentation();
    if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()); PC && SpectateTarget)
    {
        // 죽은 Pawn의 소유 연결은 HUD/호스트 재시작용으로 유지하고 카메라만 생존 팀원에게 보낸다.
        if (LastViewTarget != SpectateTarget)
        { PC->SetViewTargetWithBlend(SpectateTarget, 0.2f); LastViewTarget = SpectateTarget; }
        PC->SetControlRotation(SpectateTarget->GetBaseAimRotation());
    }
}

void USP1SurvivalComponent::InitializeMaintenanceRevive()
{
    if (!GetOwner()->HasAuthority()) return;
    Wounds = 50; LastAuthoritativeWounds = 50;
    MovementState = FSP1StaminaState(); MovementState.Current = 50;
    ServerStamina = MovementState;
}
