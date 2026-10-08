// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 신규 작성
// 변경 내용: 체력·피해·다운·구조의 서버 규칙과 상태 복제, 기존 구조 상호작용의 연결을 구현한다.

#include "SPPlayerStatusComponent.h"

#include "SPInteractableComponent.h"

#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
    bool IsPlayerPawn(const APawn* Pawn)
    {
        // 다른 플레이어의 Controller는 클라이언트에 복제되지 않으므로 PlayerState로도 확인한다.
        // 서버에서는 실제 플레이어가 빙의 중인 폰만 허용한다.
        return IsValid(Pawn) && (Pawn->IsPlayerControlled()
            || (!Pawn->HasAuthority() && Pawn->GetPlayerState() != nullptr));
    }
}

USPPlayerStatusComponent::USPPlayerStatusComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void USPPlayerStatusComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USPPlayerStatusComponent, StatusSnapshot);
}

void USPPlayerStatusComponent::BeginPlay()
{
    Super::BeginPlay();

    MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(1.0f, MaxHealth) : 100.0f;
    ReviveDuration = FMath::IsFinite(ReviveDuration) ? FMath::Max(0.1f, ReviveDuration) : 4.0f;
    ReviveHealthRatio = FMath::IsFinite(ReviveHealthRatio) ? FMath::Clamp(ReviveHealthRatio, 0.01f, 1.0f) : 0.3f;
    ReviveMovementTolerance = FMath::IsFinite(ReviveMovementTolerance) ? FMath::Max(0.0f, ReviveMovementTolerance) : 5.0f;

    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    Owner->OnTakeAnyDamage.AddDynamic(this, &USPPlayerStatusComponent::HandleAnyDamage);
    ReviveInteraction = Owner->FindComponentByClass<USPInteractableComponent>();
    if (ReviveInteraction)
    {
        ReviveInteraction->HoldDuration = ReviveDuration;
        ReviveInteraction->bKeepProgress = false;
        ReviveInteraction->bLockControls = false;
        ReviveInteraction->bCancelOnMovement = true;
        ReviveInteraction->MovementCancelTolerance = ReviveMovementTolerance;
        ReviveInteraction->bRequireLineOfSight = true;
        ReviveInteraction->Prompt = NSLOCTEXT("SpacePirate", "RevivePrompt", "구조");
        ReviveInteraction->CanInteractNative.BindUObject(this, &USPPlayerStatusComponent::CanBeRevivedBy);
        ReviveInteraction->OnCompletedNative.AddUObject(this, &USPPlayerStatusComponent::HandleReviveCompleted);
    }

    if (Owner->HasAuthority())
    {
        ResetForStage();
    }
}

void USPPlayerStatusComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CancelRevive();
    if (ReviveInteraction)
    {
        if (ReviveInteraction->CanInteractNative.IsBoundToObject(this))
        {
            ReviveInteraction->CanInteractNative.Unbind();
        }
        ReviveInteraction->OnCompletedNative.RemoveAll(this);
    }
    if (GetOwner())
    {
        GetOwner()->OnTakeAnyDamage.RemoveDynamic(this, &USPPlayerStatusComponent::HandleAnyDamage);
    }

    Super::EndPlay(EndPlayReason);
}

float USPPlayerStatusComponent::GetHealthPercent() const
{
    return MaxHealth > 0.0f ? FMath::Clamp(GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

void USPPlayerStatusComponent::HandleAnyDamage(AActor* DamagedActor, float Damage,
    const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
    ApplyDamage(Damage);
}

void USPPlayerStatusComponent::ApplyDamage(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsDowned()
        || !FMath::IsFinite(Amount) || Amount <= 0.0f)
    {
        return;
    }

    if (Amount >= StatusSnapshot.Health)
    {
        EnterDowned();
        return;
    }

    const FSPPlayerStatusSnapshot PreviousSnapshot = StatusSnapshot;
    StatusSnapshot.Health = FMath::Clamp(StatusSnapshot.Health - Amount, 0.0f, MaxHealth);
    BroadcastChanges(PreviousSnapshot);
    GetOwner()->ForceNetUpdate();
}

void USPPlayerStatusComponent::EnterDowned()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsDowned())
    {
        return;
    }

    const FSPPlayerStatusSnapshot PreviousSnapshot = StatusSnapshot;
    StatusSnapshot.Health = 0.0f;
    StatusSnapshot.State = ESPPlayerLifeState::Downed;
    BroadcastChanges(PreviousSnapshot);
    GetOwner()->ForceNetUpdate();
}

bool USPPlayerStatusComponent::CanBeRevivedBy(APawn* Rescuer) const
{
    const APawn* OwnerPawn = GetOwner<APawn>();
    if (!IsDowned() || Rescuer == OwnerPawn || !IsPlayerPawn(OwnerPawn) || !IsPlayerPawn(Rescuer))
    {
        return false;
    }

    const USPPlayerStatusComponent* RescuerLife = Rescuer->FindComponentByClass<USPPlayerStatusComponent>();
    return RescuerLife && !RescuerLife->IsDowned();
}

void USPPlayerStatusComponent::HandleReviveCompleted(APawn* Rescuer)
{
    TryRevive(Rescuer);
}

bool USPPlayerStatusComponent::TryRevive(APawn* Rescuer)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !CanBeRevivedBy(Rescuer))
    {
        return false;
    }

    CancelRevive();
    const FSPPlayerStatusSnapshot PreviousSnapshot = StatusSnapshot;
    StatusSnapshot.Health = MaxHealth * ReviveHealthRatio;
    StatusSnapshot.State = ESPPlayerLifeState::Active;
    BroadcastChanges(PreviousSnapshot);
    GetOwner()->ForceNetUpdate();
    return true;
}

void USPPlayerStatusComponent::ResetForStage()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    CancelRevive();
    const FSPPlayerStatusSnapshot PreviousSnapshot = StatusSnapshot;
    StatusSnapshot.Health = MaxHealth;
    StatusSnapshot.State = ESPPlayerLifeState::Active;
    BroadcastChanges(PreviousSnapshot, true);
    GetOwner()->ForceNetUpdate();
}

void USPPlayerStatusComponent::CancelRevive()
{
    if (GetOwner() && GetOwner()->HasAuthority() && ReviveInteraction)
    {
        ReviveInteraction->CancelBy(ReviveInteraction->GetCurrentUser());
    }
}

float USPPlayerStatusComponent::GetReviveProgress() const
{
    return IsDowned() && ReviveInteraction ? ReviveInteraction->GetProgress() : 0.0f;
}

APawn* USPPlayerStatusComponent::GetRescuer() const
{
    return IsDowned() && ReviveInteraction ? ReviveInteraction->GetCurrentUser() : nullptr;
}

void USPPlayerStatusComponent::OnRep_StatusSnapshot(const FSPPlayerStatusSnapshot& PreviousSnapshot)
{
    BroadcastChanges(PreviousSnapshot);
}

void USPPlayerStatusComponent::BroadcastChanges(const FSPPlayerStatusSnapshot& PreviousSnapshot, bool bForce)
{
    if (bForce || PreviousSnapshot.Health != StatusSnapshot.Health)
    {
        OnHealthChanged.Broadcast(StatusSnapshot.Health, MaxHealth);
    }
    if (bForce || PreviousSnapshot.State != StatusSnapshot.State)
    {
        OnLifeStateChanged.Broadcast(StatusSnapshot.State);
    }
}
