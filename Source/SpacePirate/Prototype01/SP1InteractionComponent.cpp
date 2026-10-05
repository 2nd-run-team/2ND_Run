// 작성자 : 임진혁
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1TransferCargo.h"
#include "Prototype01/SP1CargoDefinition.h"
#include "Prototype01/SP1ExtractionZone.h"
#include "Prototype01/SP1HUDWidget.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "Prototype01/SP1Bulkhead.h"
#include "Prototype01/SP1MaintenanceStation.h"
#include "SPCharacterMovementComponent.h"
#include "SPInventoryComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "InputCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/CoreDelegates.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

namespace
{
    FVector InteractionPoint(const AActor* Target)
    {
        if (const auto* Cargo = Cast<ASP1TransferCargo>(Target)) return Cargo->GetInteractionPoint();
        if (const auto* Zone = Cast<ASP1ExtractionZone>(Target)) return Zone->GetInteractionPoint();
        if (const auto* Panel = Cast<ASP1Panel>(Target)) return Panel->GetInteractionPoint();
        if (const auto* Station = Cast<ASP1MaintenanceStation>(Target)) return Station->GetInteractionPoint();
        return Target ? Target->GetActorLocation() : FVector::ZeroVector;
    }
    FName TargetId(const AActor* Target)
    {
        const auto* Cargo = Cast<ASP1TransferCargo>(Target);
        if (const auto* Panel = Cast<ASP1Panel>(Target)) return Panel->PanelId;
        if (const auto* Station = Cast<ASP1MaintenanceStation>(Target)) return Station->StationId;
        if (const auto* Zone = Cast<ASP1ExtractionZone>(Target)) return Zone->DepartureId;
        return Cargo ? Cargo->CargoId : FName(TEXT("Extraction"));
    }
}

USP1InteractionComponent::USP1InteractionComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
}

bool USP1InteractionComponent::IsLocal() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn && Pawn->IsLocallyControlled();
}

APlayerController* USP1InteractionComponent::GetPlayerController() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

bool USP1InteractionComponent::IsAlive() const
{
    const ACharacter* Pawn = Cast<ACharacter>(GetOwner());
    const auto* Life = Pawn ? Pawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
    const APlayerController* PC = GetPlayerController();
    return IsValid(Pawn) && (!Life || !Life->IsDead()) && !Pawn->IsActorBeingDestroyed() && PC && PC->GetPawn() == Pawn
        && PC->PlayerState && !PC->PlayerState->IsOnlyASpectator()
        && Pawn->GetCharacterMovement()->MovementMode != MOVE_None;
}

void USP1InteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    // 창 포커스를 잃으면 입력 해제를 기다리지 않는다. 서버에도 유지 신호 타임아웃이 별도로 있다.
    DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this, &ThisClass::OnAppDeactivated);
}

void USP1InteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
    if (GetOwner()->HasAuthority()) CancelOnServer(TEXT("DisconnectedOrDestroyed"));
    SetMovementRequest(false);
    if (HUD) HUD->RemoveFromParent();
    if (InputController.IsValid() && EvaluationInput) InputController->PopInputComponent(EvaluationInput);
    if (EvaluationInput) EvaluationInput->DestroyComponent();
    if (const auto* Inventory = GetOwner()->FindComponentByClass<USPInventoryComponent>(); GEngine && Inventory)
        GEngine->RemoveOnScreenDebugMessage(Inventory->GetUniqueID());
    Super::EndPlay(Reason);
}

void USP1InteractionComponent::BindInput(UEnhancedInputComponent* Input, UInputAction* Action)
{
    if (!Input || !Action) return;
    Input->BindAction(Action, ETriggerEvent::Started, this, &ThisClass::BeginHold);
    Input->BindAction(Action, ETriggerEvent::Triggered, this, &ThisClass::HoldTriggered);
    Input->BindAction(Action, ETriggerEvent::Completed, this, &ThisClass::EndHold);
    Input->BindAction(Action, ETriggerEvent::Canceled, this, &ThisClass::EndHold);
}

void USP1InteractionComponent::SetMovementRequest(bool bRequested)
{
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        if (auto* Move = Cast<USPCharacterMovementComponent>(Character->GetCharacterMovement()))
            Move->SetInteractionMovementRequested(bRequested);
}

void USP1InteractionComponent::OnAppDeactivated()
{
    if (IsLocal() && bLocalHeld) { EndHold(); LocalReason = TEXT("FocusLost"); }
}

void USP1InteractionComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    if (!IsLocal()) return;
    InstallEvaluationInput();
    // 공용 운반 디버그 자체는 보존하고 이 평가 Pawn의 메시지 키만 제거한다.
    if (const auto* Inventory = GetOwner()->FindComponentByClass<USPInventoryComponent>(); GEngine && Inventory)
        GEngine->RemoveOnScreenDebugMessage(Inventory->GetUniqueID());
    APlayerController* PC = GetPlayerController();
    if (!HUD && PC && HUDClass)
    {
        HUD = CreateWidget<USP1HUDWidget>(PC, HUDClass);
        if (HUD) HUD->AddToViewport();
    }
    USP1RoundComponent* Round = USP1RoundComponent::Find(this);
    if (Round && SP1::IsTerminal(Round->State.Phase))
    {
        // 종료 스냅샷을 받은 소유 클라이언트도 즉시 입력 예측을 멈춘다.
        if (auto* Character = Cast<ACharacter>(GetOwner())) Character->GetCharacterMovement()->DisableMovement();
    }
    FocusTarget = IsAlive() ? FindFocus() : nullptr;
    if (!bLocalHeld) return;
    if (!Round || Round->State.RunId != LocalRunId || !SP1::IsPlaying(Round->State.Phase) || !IsAlive())
    { EndHold(); return; }
    if (FocusTarget && FocusTarget != LocalTarget)
    { EndHold(); LocalReason = TEXT("TargetChanged"); return; }
    const double Time = GetWorld()->GetTimeSeconds();
    // Triggered가 멈춘 상태에서 Tick만으로 신호를 만들지 않는다(포커스 상실/키 해제 방어).
    if (Time - LastTriggerAt <= 0.12 && Time - LastPulseAt >= FMath::Clamp(HeartbeatInterval, 0.05f, 0.5f))
    {
        LastPulseAt = Time;
        ServerHeartbeat(LocalRunId, LocalRequestId, Attempt.RequestId == LocalRequestId ? Attempt.AttemptId : 0);
    }
}

void USP1InteractionComponent::InstallEvaluationInput()
{
    APlayerController* PC = GetPlayerController();
    if (!PC || EvaluationInput) return;
    // 전용 Pawn에서만 상위 우선순위의 작은 입력 계층을 설치한다. 다른 맵의 키는 건드리지 않는다.
    EvaluationInput = NewObject<UInputComponent>(GetOwner(),TEXT("SP1EvaluationInput"));
    EvaluationInput->RegisterComponent();
    EvaluationInput->Priority = 1000;
    EvaluationInput->BindKey(EKeys::MiddleMouseButton,IE_Pressed,this,&ThisClass::RequestPing).bConsumeInput = true;
    for (FKey Key : {EKeys::F6,EKeys::F7,EKeys::F8,EKeys::F9,EKeys::Zero,EKeys::NumPadZero})
        EvaluationInput->BindKey(Key,IE_Pressed,this,&ThisClass::BlockDebugKey).bConsumeInput = true;
    PC->PushInputComponent(EvaluationInput);
    InputController = PC;
}
void USP1InteractionComponent::BlockDebugKey() { LocalReason = TEXT("EvaluationDebugBlocked"); }
void USP1InteractionComponent::RequestPing()
{
    if (IsLocal() && IsAlive()) if (const auto* Round = USP1RoundComponent::Find(this)) ServerPing(Round->State.RunId);
}
void USP1InteractionComponent::ServerPing_Implementation(FGuid RunId)
{
    if (auto* Round = USP1RoundComponent::Find(this)) Round->RequestPing(this,RunId);
}

AActor* USP1InteractionComponent::FindFocus() const
{
    const APlayerController* PC = GetPlayerController();
    if (!PC) return nullptr;
    FVector Eye;
    FRotator View;
    PC->GetPlayerViewPoint(Eye, View);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SP1Focus), false, GetOwner());
    const float Range = bLocalHeld ? MaintainDistance : StartDistance;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * Range, ECC_Visibility, Query)) return nullptr;
    if (const auto* Cargo = Cast<ASP1TransferCargo>(Hit.GetActor())) return Cargo->bTransferred ? nullptr : Hit.GetActor();
    if (const auto* Panel = Cast<ASP1Panel>(Hit.GetActor())) return Panel->Bulkhead && !Panel->Bulkhead->bOpen ? Hit.GetActor() : nullptr;
    if (Cast<ASP1MaintenanceStation>(Hit.GetActor())) return Hit.GetActor();
    return Cast<ASP1ExtractionZone>(Hit.GetActor());
}

void USP1InteractionComponent::BeginHold()
{
    if (!IsLocal() || bLocalHeld) return;
    USP1RoundComponent* Round = USP1RoundComponent::Find(this);
    if (!Round || !SP1::IsPlaying(Round->State.Phase) || !IsAlive()) return;
    FocusTarget = FindFocus();
    if (!FocusTarget) { LocalReason = TEXT("NoTarget"); return; }
    bLocalHeld = true;
    LocalReason = NAME_None;
    LocalRunId = Round->State.RunId;
    LocalRequestId = LocalRequestId == MAX_int32 ? 1 : LocalRequestId + 1;
    LocalTarget = FocusTarget;
    LastTriggerAt = GetWorld()->GetTimeSeconds();
    LastPulseAt = LastTriggerAt;
    SetMovementRequest(true);
    ServerStart(LocalRunId, LocalRequestId, LocalTarget);
}

void USP1InteractionComponent::HoldTriggered()
{
    if (IsLocal() && bLocalHeld) LastTriggerAt = GetWorld()->GetTimeSeconds();
}

void USP1InteractionComponent::EndHold()
{
    if (!IsLocal() || !bLocalHeld) return;
    bLocalHeld = false;
    SetMovementRequest(false);
    LocalReason = TEXT("Released");
    ServerCancel(LocalRunId, LocalRequestId, Attempt.RequestId == LocalRequestId ? Attempt.AttemptId : 0);
}

FName USP1InteractionComponent::ValidateTarget(AActor* Target, bool bStarting, bool bStrictAim, double Time)
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const auto* Round = USP1RoundComponent::Find(this);
    if (!IsAlive()) return TEXT("NotAlive");
    if (!Round || !Round->CanInteract(Pawn)) return TEXT("RoundUnavailable");
    if (!IsValid(Target)) return TEXT("TargetMissing");
    if (const auto* Cargo = Cast<ASP1TransferCargo>(Target))
    {
        if (Cargo->bTransferred) return TEXT("AlreadyTransferred");
        if (!Cargo->Definition) return TEXT("InvalidDefinition");
    }
    else if (const auto* Zone = Cast<ASP1ExtractionZone>(Target))
    {
        if (!Round->CanUseExtraction(Zone)) return Zone->bIntermediate && !Round->CanUseMaintenance() ? TEXT("MaintenanceClosed") : TEXT("DepartureAlreadyStarted");
        if (!Zone->ContainsPawn(Pawn)) return TEXT("OutsideExtraction");
    }
    else if (const auto* Panel = Cast<ASP1Panel>(Target))
    {
        if (!Panel->Bulkhead || Panel->Bulkhead->bOpen) return TEXT("DeviceUnavailable");
    }
    else if (Cast<ASP1MaintenanceStation>(Target))
    {
        if (Target != Round->MaintenanceStation || !Round->CanUseMaintenance()) return TEXT("MaintenanceClosed");
    }
    else return TEXT("UnsupportedTarget");

    const UCameraComponent* Camera = Pawn->FindComponentByClass<UCameraComponent>();
    const FVector Eye = Camera ? Camera->GetComponentLocation() : Pawn->GetPawnViewLocation();
    const FVector Point = InteractionPoint(Target);
    const FVector ToPoint = Point - Eye;
    const float Range = FMath::Clamp(bStarting ? StartDistance : MaintainDistance, 1.0f, 500.0f);
    if (ToPoint.SizeSquared() > FMath::Square(Range)) return TEXT("OutOfRange");
    const FVector Aim = Pawn->GetBaseAimRotation().Vector();
    const double AimDot = FVector::DotProduct(Aim, ToPoint.GetSafeNormal());
    const bool bGoodAim = AimDot >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(AimAngleDegrees, 1.0f, 45.0f)));
    if (bGoodAim) LastValidAimAt = Time;
    else if (bStarting || bStrictAim || Time - LastValidAimAt > FMath::Clamp(AimGraceSeconds, 0.0f, 0.5f)) return TEXT("LookAway");

    FCollisionQueryParams Query(SCENE_QUERY_STAT(SP1ServerVisibility), false, Pawn);
    FHitResult Hit;
    // 벽은 유예 없이 취소한다. 시선 유예를 가림 허용 시간으로 사용하지 않는다.
    if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Query) && Hit.GetActor() != Target) return TEXT("Occluded");
    if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + Aim * Range, ECC_Visibility, Query)
        && Hit.GetActor() != Target && (Cast<ASP1TransferCargo>(Hit.GetActor()) || Cast<ASP1ExtractionZone>(Hit.GetActor()) || Cast<ASP1Panel>(Hit.GetActor()) || Cast<ASP1MaintenanceStation>(Hit.GetActor()))) return TEXT("TargetChanged");
    return NAME_None;
}

void USP1InteractionComponent::ServerStart_Implementation(FGuid RunId, int32 RequestId, AActor* Target)
{
    USP1RoundComponent* Round = USP1RoundComponent::Find(this);
    if (!Round || RunId != Round->State.RunId || RequestId <= LastServerRequestId || RequestId <= 0) return;
    LastServerRequestId = RequestId;
    CancelOnServer(TEXT("Replaced"));
    Attempt = FSP1Attempt();
    Attempt.RunId = RunId;
    Attempt.RequestId = RequestId;
    Attempt.AttemptId = ++NextAttemptId;
    Attempt.Target = Target;
    const double Time = Round->Now();
    Attempt.Reason = ValidateTarget(Target, true, true, Time);
    if (Attempt.Reason.IsNone())
        if (const auto* Panel = Cast<ASP1Panel>(Target)) Attempt.Reason = Panel->Bulkhead->TryClaim(this,Panel);
    if (Attempt.Reason.IsNone())
    {
        Attempt.bActive = true;
        Attempt.StartedAt = Time;
        if (const auto* Panel = Cast<ASP1Panel>(Target)) Attempt.Duration = Panel->Kind == ESP1PanelKind::Bypass ? Panel->Bulkhead->BypassSeconds : Panel->Bulkhead->TogetherSeconds;
        else if (const auto* Station = Cast<ASP1MaintenanceStation>(Target)) Attempt.Duration = Station->HoldSeconds;
        else Attempt.Duration = Cast<ASP1TransferCargo>(Target) ? Cast<ASP1TransferCargo>(Target)->Definition->HoldSeconds : Cast<ASP1ExtractionZone>(Target)->HoldSeconds;
        Attempt.Duration = FMath::Clamp(Attempt.Duration, 0.2f, 120.0f);
        LastServerPulseAt = Time;
        LastValidAimAt = Time;
        Round->LogEvent(TEXT("HoldStart"), this, TargetId(Target));
    }
    else Round->LogEvent(TEXT("HoldRejected"), this, TargetId(Target), Attempt.Reason.ToString());
    OnRep_Attempt();
    GetOwner()->ForceNetUpdate();
}

void USP1InteractionComponent::ServerCancel_Implementation(FGuid RunId, int32 RequestId, int32 AttemptId)
{
    if (SP1::MatchesAttempt(Attempt, RunId, RequestId, AttemptId)) CancelOnServer(TEXT("CanceledByInput"));
}

void USP1InteractionComponent::ServerHeartbeat_Implementation(FGuid RunId, int32 RequestId, int32 AttemptId)
{
    if (!SP1::MatchesAttempt(Attempt, RunId, RequestId, AttemptId)) return;
    if (const auto* Round = USP1RoundComponent::Find(this))
    {
        const double Time = Round->Now();
        // 이미 타임아웃인 시도는 늦은 신호로 되살리지 않는다.
        if (Time - LastServerPulseAt > FMath::Clamp(HeartbeatTimeout, 0.2f, 3.0f)) { CancelOnServer(TEXT("HeartbeatTimeout")); return; }
        LastServerPulseAt = Time;
    }
}

void USP1InteractionComponent::EvaluateOnServer(double Time)
{
    if (!GetOwner()->HasAuthority() || !Attempt.bActive) return;
    // 패널은 개별 시작 시각으로 완료하지 않는다. 두 시도가 모두 유효해진 뒤 격벽이 공동 시간을 판정한다.
    if (Cast<ASP1Panel>(Attempt.Target)) { ValidateDeviceHold(Time,false); return; }
    if (Time - LastServerPulseAt > FMath::Clamp(HeartbeatTimeout, 0.2f, 3.0f)) { CancelOnServer(TEXT("HeartbeatTimeout")); return; }
    const bool bDue = Time >= Attempt.StartedAt + Attempt.Duration;
    const FName Invalid = ValidateTarget(Attempt.Target, false, bDue, Time);
    if (!Invalid.IsNone()) { CancelOnServer(Invalid); return; }
    if (!bDue) return;
    auto* Round = USP1RoundComponent::Find(this);
    const bool bCompleted = Round && Round->CompleteTarget(this);
    CancelOnServer(bCompleted ? TEXT("Completed") : TEXT("AlreadyResolved"));
}

bool USP1InteractionComponent::ValidateDeviceHold(double Time, bool bStrictAim)
{
    if (!GetOwner()->HasAuthority() || !Attempt.bActive) return false;
    FName Invalid = Time-LastServerPulseAt > FMath::Clamp(HeartbeatTimeout,.2f,3.f)
        ? FName(TEXT("HeartbeatTimeout")) : ValidateTarget(Attempt.Target,false,bStrictAim,Time);
    if (!Invalid.IsNone()) { CancelOnServer(Invalid); return false; }
    return true;
}

void USP1InteractionComponent::CancelOnServer(FName Reason)
{
    if (!GetOwner()->HasAuthority() || !Attempt.bActive) return;
    if (const auto* Panel = Cast<ASP1Panel>(Attempt.Target); Panel && Panel->Bulkhead) Panel->Bulkhead->Release(this);
    if (const auto* Round = USP1RoundComponent::Find(this))
        Round->LogEvent(Reason == TEXT("Completed") ? TEXT("HoldCompleted") : TEXT("HoldCancel"), this, TargetId(Attempt.Target), Reason.ToString());
    Attempt.bActive = false;
    Attempt.Reason = Reason;
    OnRep_Attempt();
    GetOwner()->ForceNetUpdate();
}

void USP1InteractionComponent::OnRep_Attempt()
{
    if (!IsLocal() || Attempt.RequestId != LocalRequestId || Attempt.RunId != LocalRunId) return;
    if (!Attempt.bActive)
    {
        bLocalHeld = false;
        LocalReason = Attempt.Reason;
        SetMovementRequest(false);
    }
}

void USP1InteractionComponent::ResetForRun()
{
    CancelOnServer(TEXT("Restart"));
    Attempt = FSP1Attempt();
    bLocalHeld = false;
    SetMovementRequest(false);
}

float USP1InteractionComponent::GetProgress() const
{
    const auto* Round = USP1RoundComponent::Find(this);
    if (const auto* Panel = Cast<ASP1Panel>(Attempt.Target); Panel && Panel->Bulkhead && Round && bLocalHeld && Attempt.bActive)
        return Panel->Bulkhead->GetProgress(Round->Now());
    return Round && bLocalHeld && Attempt.bActive && Attempt.RequestId == LocalRequestId && Attempt.Duration > 0
        ? FMath::Clamp(static_cast<float>((Round->Now() - Attempt.StartedAt) / Attempt.Duration), 0.0f, 1.0f) : 0;
}

void USP1InteractionComponent::ToggleReady()
{
    const auto* Round = USP1RoundComponent::Find(this);
    const auto* PC = GetPlayerController();
    if (!IsLocal() || !Round || !PC) return;
    const auto* P = Round->Participants.FindByPredicate([PC](const FSP1Participant& Row) { return Row.PlayerState == PC->PlayerState; });
    ServerReady(Round->State.RunId, !(P && P->bReady));
}
void USP1InteractionComponent::RequestStart()
{ if (const auto* Round = USP1RoundComponent::Find(this); IsLocal() && Round) ServerStartRun(Round->State.RunId); }
void USP1InteractionComponent::RequestRestart()
{ if (const auto* Round = USP1RoundComponent::Find(this); IsLocal() && Round) ServerRestartRun(Round->State.RunId); }
void USP1InteractionComponent::ServerReady_Implementation(FGuid RunId, bool bReady)
{ if (auto* Round = USP1RoundComponent::Find(this)) Round->SetReady(GetPlayerController(), RunId, bReady); }
void USP1InteractionComponent::ServerStartRun_Implementation(FGuid RunId)
{ if (auto* Round = USP1RoundComponent::Find(this)) Round->StartRun(GetPlayerController(), RunId); }
void USP1InteractionComponent::ServerRestartRun_Implementation(FGuid RunId)
{ if (auto* Round = USP1RoundComponent::Find(this)) Round->RestartRun(GetPlayerController(), RunId); }

void USP1InteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(USP1InteractionComponent, Attempt, COND_OwnerOnly);
}
