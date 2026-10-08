// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 다운된 조작자를 거부하고 이동·시야 차단에 따른 취소와 거리 입력값 검증을 추가한다.

#include "SPInteractableComponent.h"

#include "SPDebug.h"
#include "SPStealthActivityComponent.h"
#include "SPPlayerStatusComponent.h"
#include "Camera/CameraComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

USPInteractableComponent::USPInteractableComponent()
{
    // 누르는 동안 서버에서만 켠다.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void USPInteractableComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USPInteractableComponent, CurrentUser);
    DOREPLIFETIME(USPInteractableComponent, StartTime);
    DOREPLIFETIME(USPInteractableComponent, SavedSeconds);
}

void USPInteractableComponent::BeginPlay()
{
    Super::BeginPlay();

    // 액터가 복제되지 않으면 클라이언트가 조작자와 진행률을 받지 못하고, 서버 RPC로 대상을 넘길 수도 없다.
    if (GetOwner() && !GetOwner()->GetIsReplicated())
    {
        SP_DEBUG_LOG(Warning, TEXT("%s: Interactable owner does not replicate. Enable Replicates on the actor so clients can use it."),
            *GetNameSafe(GetOwner()));
    }
}

bool USPInteractableComponent::CanInteract_Implementation(APawn* User) const
{
    return IsUserAvailable(User) && (!CanInteractNative.IsBound() || CanInteractNative.Execute(User));
}

bool USPInteractableComponent::IsUserAvailable(const APawn* User) const
{
    if (!IsValid(User))
    {
        return false;
    }

    const USPPlayerStatusComponent* Life = User->FindComponentByClass<USPPlayerStatusComponent>();
    return !Life || !Life->IsDowned();
}

bool USPInteractableComponent::HasLineOfSight(const APawn* User) const
{
    if (!bRequireLineOfSight)
    {
        return true;
    }
    if (!User || !GetOwner() || !GetWorld())
    {
        return false;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(SPInteractLineOfSight), false, User);
    Params.AddIgnoredActor(GetOwner());
    const UCameraComponent* Camera = User->FindComponentByClass<UCameraComponent>();
    const FVector ViewLocation = Camera ? Camera->GetComponentLocation() : User->GetPawnViewLocation();
    return !GetWorld()->LineTraceTestByChannel(ViewLocation, GetOwner()->GetActorLocation(),
        ECC_Visibility, Params);
}

bool USPInteractableComponent::TryInteract(APawn* User, float MaxDistance)
{
    // 동시 요청의 패배, 거리 초과, 조건 불충족은 정상 흐름이라 기록하지 않는다.
    if (!ensure(GetOwner() && GetOwner()->HasAuthority())
        || bTransitioning
        || !IsUserAvailable(User)
        || CurrentUser
        || !FMath::IsFinite(MaxDistance) || MaxDistance < 0.0f
        || User->GetDistanceTo(GetOwner()) > MaxDistance
        || !HasLineOfSight(User)
        || !CanInteract(User))
    {
        return false;
    }
    auto* Activity=User->FindComponentByClass<USPStealthActivityComponent>();
    if (Activity && Activity->IsIncapacitated()) { return false; }

    CurrentUser = User;
    StartTime = GetSyncedTime();
    HeldSeconds = 0.0f;
    AllowedDistance = MaxDistance;
    UserStartLocation = User->GetActorLocation();
    bStartedWithController = User->GetController()!=nullptr;
    // 명시적으로 지정한 지속 작업만 등록한다. 일반 E 상호작용은 CrimeKind=None을 유지한다.
    // 시작 알림보다 먼저 등록해야 알림 안에서 취소해도 Finish가 같은 등록을 회수할 수 있다.
    if (Activity && CrimeKind!=ESPCrimeKind::None && !bInstantCrime && !IsInstant())
    { CrimeRegistration=Activity->BeginCrime(CrimeKind,this); }
    OnStartedNative.Broadcast(User);
    OnStarted.Broadcast(User);
    // 시작 콜백에서 작업을 취소하거나 대상을 파괴할 수 있으므로, 완료·Tick 활성화 전에 다시 확인한다.
    if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed() || CurrentUser!=User) { return false; }
    if (IsInstant()) { Finish(true); return false; }
    SetComponentTickEnabled(true);
    return CurrentUser==User;
}

void USPInteractableComponent::CancelBy(const APawn* User)
{
    if (GetOwner() && GetOwner()->HasAuthority() && User && CurrentUser == User)
    {
        Finish(false);
    }
}

void USPInteractableComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!GetOwner() || !GetOwner()->HasAuthority() || !CurrentUser) { return; }
    const auto* Activity=CurrentUser->FindComponentByClass<USPStealthActivityComponent>();

    // ponytail: 거리는 액터 원점 기준이다. 원점이 표면에서 먼 큰 대상(긴 단말대 등)이 생기면 바운드 기준으로 바꾼다.
    if (!IsUserAvailable(CurrentUser)
        || (bStartedWithController && !CurrentUser->GetController())
        || (Activity && Activity->IsIncapacitated())
        || CurrentUser->GetDistanceTo(GetOwner()) > AllowedDistance
        || (bCancelOnMovement && FVector::DistSquared(CurrentUser->GetActorLocation(), UserStartLocation)
            > FMath::Square(FMath::Max(0.0f, MovementCancelTolerance)))
        || !HasLineOfSight(CurrentUser)
        || !CanInteract(CurrentUser))
    {
        Finish(false);
        return;
    }

    HeldSeconds += DeltaTime;
    if (SavedSeconds + HeldSeconds >= HoldDuration)
    {
        Finish(true);
    }
}

void USPInteractableComponent::Finish(bool bCompleted)
{
    if (bTransitioning || !CurrentUser || !GetOwner() || !GetOwner()->HasAuthority()) { return; }
    // 완료/취소 콜백의 재진입으로 보상 지급이나 범죄 정리가 반복되지 않도록 전환 중 요청을 막는다.
    TGuardValue<bool> Transition(bTransitioning,true);
    APawn* User = CurrentUser;
    auto* Activity=IsValid(User)?User->FindComponentByClass<USPStealthActivityComponent>():nullptr;
    if (Activity)
    {
        // 성공·취소 모두 지속 범죄를 먼저 해제한다. 이후 콜백이 대상을 없애도 등록이 남지 않는다.
        Activity->EndCrime(CrimeRegistration);
        if (bCompleted && bInstantCrime && CrimeKind!=ESPCrimeKind::None) { Activity->ReportInstantCrime(CrimeKind,this); }
    }
    CrimeRegistration.Invalidate();

    // 완료되면 처음부터, 취소되면 보존 대상만 쌓인 시간을 남긴다.
    SavedSeconds = (!bCompleted && bKeepProgress) ? SavedSeconds + HeldSeconds : 0.0f;
    HeldSeconds = 0.0f;
    CurrentUser = nullptr;
    SetComponentTickEnabled(false);

    if (bCompleted)
    {
        OnCompleted.Broadcast(User);
        OnCompletedNative.Broadcast(User);
    }
    else
    {
        OnCancelledNative.Broadcast(User);
        OnCancelled.Broadcast(User);
    }
}

void USPInteractableComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner() && GetOwner()->HasAuthority()) { Finish(false); }
    Super::EndPlay(Reason);
}

float USPInteractableComponent::GetProgress() const
{
    if (IsInstant())
    {
        return 0.0f;
    }

    // 서버는 누른 시간을 직접 세고, 클라이언트는 복제된 시작 시각과 서버 시간으로 계산한다.
    float Seconds = SavedSeconds;
    if (CurrentUser)
    {
        Seconds += (GetOwner() && GetOwner()->HasAuthority())
            ? HeldSeconds
            : static_cast<float>(GetSyncedTime() - StartTime);
    }

    return FMath::Clamp(Seconds / HoldDuration, 0.0f, 1.0f);
}

double USPInteractableComponent::GetSyncedTime() const
{
    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
