// 작성자 : 임진혁
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1TransferCargo.h"
#include "Prototype01/SP1CargoDefinition.h"
#include "Prototype01/SP1ExtractionZone.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "Prototype01/SP1LaserHazard.h"
#include "Prototype01/SP1Bulkhead.h"
#include "Prototype01/SP1GravityCargo.h"
#include "Prototype01/SP1SecurityCamera.h"
#include "Prototype01/SP1CarRegion.h"
#include "Prototype01/SP1MaintenanceStation.h"
#include "Prototype01/SP1ScenarioDefinition.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

USP1RoundComponent::USP1RoundComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

USP1RoundComponent* USP1RoundComponent::Find(const UObject* Context)
{
    const UWorld* World = Context ? Context->GetWorld() : nullptr;
    const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
    return GS ? GS->FindComponentByClass<USP1RoundComponent>() : nullptr;
}

double USP1RoundComponent::Now() const
{
    const AGameStateBase* GS = Cast<AGameStateBase>(GetOwner());
    return GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

void USP1RoundComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()->HasAuthority()) ResetRun(false);
}

void USP1RoundComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner()->HasAuthority() && SP1::IsPlaying(State.Phase))
        Finish(ESP1Phase::Aborted, TEXT("Host or world closed"));
    Super::EndPlay(Reason);
}

void USP1RoundComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USP1RoundComponent, State);
    DOREPLIFETIME(USP1RoundComponent, Participants);
    DOREPLIFETIME(USP1RoundComponent, Pings);
    DOREPLIFETIME(USP1RoundComponent, Extraction);
    DOREPLIFETIME(USP1RoundComponent, MaintenanceStation);
}

void USP1RoundComponent::ResetRun(bool bResetPawns)
{
    Ledger = FSP1RunLedger();
    Ledger.RunId = FGuid::NewGuid();
    State = FSP1RunState();
    State.RunId = Ledger.RunId;
    Participants.Reset();
    Pings.Reset(); NextPingAt.Reset(); NextPingId = 0; PreviousCars.Reset(); LogSequence = 0;
    PendingDamage.Reset();
    Cargo.Reset();
    Extraction = nullptr;
    MaintenanceStation = nullptr;
    Exits.Reset();
    bConfigurationValid = true;
    TSet<FName> SeenIds;
    for (TActorIterator<ASP1TransferCargo> It(GetWorld()); It; ++It)
    {
        ASP1TransferCargo* Item = *It;
        Cargo.Add(Item);
        Item->SetTransferred(false);
        if (Item->CargoId.IsNone() || SeenIds.Contains(Item->CargoId) || !Item->Definition
            || Item->Definition->Value < 0 || !FMath::IsFinite(Item->Definition->HoldSeconds)
            || Item->Definition->HoldSeconds < 0.2f) bConfigurationValid = false;
        SeenIds.Add(Item->CargoId);
        if (const auto* Special = Cast<ASP1GravityCargo>(Item); Special && !Special->GravityZone) bConfigurationValid = false;
    }
    for (TActorIterator<ASP1Bulkhead> It(GetWorld()); It; ++It) It->ResetForRun();
    for (TActorIterator<ASP1SecurityCamera> It(GetWorld()); It; ++It)
    { It->ResetForRun(State.RunId,Now()); if (!It->Region) bConfigurationValid = false; }
    TSet<FName> ExitIds;
    int32 FinalCount = 0, MidCount = 0, StationCount = 0;
    for (TActorIterator<ASP1ExtractionZone> It(GetWorld()); It; ++It)
    {
        Exits.Add(*It);
        bConfigurationValid &= !It->DepartureId.IsNone() && !ExitIds.Contains(It->DepartureId);
        ExitIds.Add(It->DepartureId);
        if (It->bIntermediate) ++MidCount; else ++FinalCount;
    }
    for (TActorIterator<ASP1MaintenanceStation> It(GetWorld()); It; ++It) { MaintenanceStation = *It; ++StationCount; }
    bConfigurationValid &= FinalCount == 1 && Cargo.Num() > 0 && MissionSeconds > 0 && DepartureSeconds > 0;
    bConfigurationValid &= Scenario ? MidCount == 1 && StationCount == 1 && MaintenanceStation->Region
        && MaintenanceStation->ReviveOffsets.Num() >= 4 : StationCount <= 1;
    ValidateScenario();
    if (!bConfigurationValid) State.Reason = TEXT("Configuration error: cargo budget, car regions, maintenance or exits");

    // 같은 맵에서 모든 P1 상태를 새 RunId로 교체하고 Pawn을 다시 스폰한다.
    // 지연된 이전 판의 RPC는 새 RunId 검사를 통과하지 못한다.
    if (bResetPawns)
    {
        AGameModeBase* GM = GetWorld()->GetAuthGameMode();
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (!PC) continue;
            if (APawn* Pawn = PC->GetPawn()) Pawn->Destroy();
            PC->ResetIgnoreMoveInput();
            PC->ResetIgnoreLookInput();
            if (PC->PlayerState) PC->PlayerState->SetIsOnlyASpectator(false);
            if (GM) GM->RestartPlayer(PC);
        }
    }
    RefreshParticipants();
    SelectRunSettings();
    GetOwner()->ForceNetUpdate();
    LogEvent(TEXT("RunReady"));
}

APlayerController* USP1RoundComponent::FindController(int32 PlayerId) const
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (APlayerController* PC = It->Get(); PC && PC->PlayerState && PC->PlayerState->GetPlayerId() == PlayerId) return PC;
    return nullptr;
}

void USP1RoundComponent::RefreshParticipants()
{
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->PlayerState) continue;
        const int32 Id = PC->PlayerState->GetPlayerId();
        if (Participants.ContainsByPredicate([Id](const FSP1Participant& P) { return P.PlayerId == Id; })) continue;
        if (State.Phase == ESP1Phase::Ready)
        {
            FSP1Participant P;
            P.PlayerId = Id;
            P.PlayerState = PC->PlayerState;
            Participants.Add(P);
        }
        else if (PC->GetPawn())
        {
            // 진행 중 새 접속/재접속은 참가시키지 않는다. 기존 참가자의 탈출 구역 합류는 허용한다.
            PC->GetPawn()->Destroy();
            PC->StartSpectatingOnly();
        }
    }
    State.Living = 0;
    State.Aboard = 0;
    for (FSP1Participant& P : Participants)
    {
        APlayerController* PC = FindController(P.PlayerId);
        const bool bWasConnected = P.bConnected;
        P.bConnected = PC != nullptr;
        const USP1InteractionComponent* Interaction = PC && PC->GetPawn()
            ? PC->GetPawn()->FindComponentByClass<USP1InteractionComponent>() : nullptr;
        P.bAlive = P.bConnected && Interaction && Interaction->IsAlive();
        if (bWasConnected && !P.bConnected) LogEvent(TEXT("Disconnected"), nullptr, NAME_None, FString::FromInt(P.PlayerId));
        if (P.bAlive)
        {
            ++State.Living;
            if (Extraction && Extraction->ContainsPawn(PC->GetPawn())) ++State.Aboard;
        }
    }
}

bool USP1RoundComponent::CanInteract(const APawn* Pawn) const
{
    if (!Pawn || !SP1::IsPlaying(State.Phase) || SP1::MissionRemaining(State,Now()) <= 0) return false;
    const APlayerState* PS = Pawn->GetPlayerState();
    const FSP1Participant* P = PS ? Participants.FindByPredicate([PS](const FSP1Participant& Row)
        { return Row.PlayerState == PS; }) : nullptr;
    return P && P->bConnected && P->bAlive;
}

void USP1RoundComponent::SetReady(APlayerController* PC, const FGuid& RunId, bool bReady)
{
    if (!GetOwner()->HasAuthority() || RunId != State.RunId || State.Phase != ESP1Phase::Ready || !PC) return;
    RefreshParticipants();
    for (FSP1Participant& P : Participants)
        if (P.PlayerState == PC->PlayerState && P.bAlive) P.bReady = bReady;
    GetOwner()->ForceNetUpdate();
}

void USP1RoundComponent::StartRun(APlayerController* PC, const FGuid& RunId)
{
    if (!GetOwner()->HasAuthority() || !PC || !PC->IsLocalController() || RunId != State.RunId
        || State.Phase != ESP1Phase::Ready || !bConfigurationValid) return;
    RefreshParticipants();
    if (Participants.IsEmpty() || State.Living == 0) return;
    for (const FSP1Participant& P : Participants) if (P.bConnected && (!P.bReady || !P.bAlive)) return;
    State.Phase = ESP1Phase::Running;
    SelectRunSettings();
    State.MissionDeadline = Now() + State.MissionLimit;
    for (TActorIterator<ASP1LaserHazard> It(GetWorld()); It; ++It) It->StartCycle(State.RunId, Now());
    for (TActorIterator<ASP1SecurityCamera> It(GetWorld()); It; ++It) It->ResetForRun(State.RunId,Now());
    State.Reason.Reset();
    LogEvent(TEXT("RunStart"));
    GetOwner()->ForceNetUpdate();
}

void USP1RoundComponent::RestartRun(APlayerController* PC, const FGuid& RunId)
{
    if (!GetOwner()->HasAuthority() || !PC || !PC->IsLocalController() || RunId != State.RunId || !SP1::IsTerminal(State.Phase)) return;
    ResetRun(true);
}

void USP1RoundComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
    Super::TickComponent(Delta, Type, Function);
    if (!GetOwner()->HasAuthority() || SP1::IsTerminal(State.Phase)) return;
    RefreshParticipants();
    UpdatePresentationState();
    if (!SP1::IsPlaying(State.Phase)) { SelectRunSettings(); return; }
    const double Time = Now();
    // 마감/정비 종료 → 완료 → 피해/사망 → 생존/탑승 → 출발. 정비 경계는 작업 완료보다 먼저 판정한다.
    if (SP1::MissionRemaining(State,Time) <= 0) { Finish(ESP1Phase::Failed, TEXT("MissionExpired")); return; }
    UpdateMaintenance(Time);
    if (SP1::MissionRemaining(State,Time) <= 0) { Finish(ESP1Phase::Failed, TEXT("MissionExpired")); return; }
    TArray<USP1InteractionComponent*> Active;
    for (const FSP1Participant& P : Participants)
        if (APlayerController* PC = FindController(P.PlayerId); PC && PC->GetPawn())
            if (auto* Interaction = PC->GetPawn()->FindComponentByClass<USP1InteractionComponent>()) Active.Add(Interaction);
    Active.Sort([](const USP1InteractionComponent& A, const USP1InteractionComponent& B)
    {
        const double AEnd = A.Attempt.StartedAt + A.Attempt.Duration;
        const double BEnd = B.Attempt.StartedAt + B.Attempt.Duration;
        return AEnd != BEnd ? AEnd < BEnd : A.GetOwner()->GetUniqueID() < B.GetOwner()->GetUniqueID();
    });
    for (USP1InteractionComponent* Interaction : Active) if (IsValid(Interaction)) Interaction->EvaluateOnServer(Time);
    for (TActorIterator<ASP1Bulkhead> It(GetWorld()); It; ++It) It->EvaluateOnServer(Time);
    for (TActorIterator<ASP1LaserHazard> It(GetWorld()); It; ++It) It->EvaluateDamage(Time);
    for (TActorIterator<ASP1SecurityCamera> It(GetWorld()); It; ++It) It->EvaluateOnServer(Time,Delta);
    TArray<FPendingDamage> Damage = MoveTemp(PendingDamage);
    PendingDamage.Reset();
    for (const auto& Hit : Damage) if (auto* Life = Hit.Target.Get()) Life->ApplyWound(Hit.Amount, Hit.HazardId);
    RefreshParticipants();
    for (const FSP1Participant& P : Participants)
        if (APlayerController* PC = FindController(P.PlayerId); PC && PC->GetPawn())
            if (auto* Life = PC->GetPawn()->FindComponentByClass<USP1SurvivalComponent>()) Life->UpdateSpectator();
    const double EffectiveDeadline = State.Maintenance.Phase == ESP1MaintenancePhase::Active
        ? Time + State.Maintenance.FrozenMissionRemaining : State.MissionDeadline;
    if (const auto End = SP1::ResolveEnd(State.Phase, Time, EffectiveDeadline,
        State.ExtractionDeadline, State.Living, State.Aboard); End.IsSet())
        Finish(End->Phase, End->Reason.ToString());
}

void USP1RoundComponent::QueueDamage(USP1SurvivalComponent* Target, float Amount, FName HazardId)
{
    if (!GetOwner()->HasAuthority() || !Target || Target->IsDead() || !FMath::IsFinite(Amount) || Amount <= 0
        || !CanInteract(Cast<APawn>(Target->GetOwner())) || IsHazardProtected(Cast<APawn>(Target->GetOwner()))) return;
    PendingDamage.Add({Target, FMath::Min(100.0f,Amount), HazardId});
}

bool USP1RoundComponent::CompleteTarget(USP1InteractionComponent* Source)
{
    if (!GetOwner()->HasAuthority() || !Source || Source->Attempt.RunId != State.RunId
        || !CanInteract(Cast<APawn>(Source->GetOwner()))) return false;
    if (ASP1TransferCargo* Item = Cast<ASP1TransferCargo>(Source->Attempt.Target))
    {
        if (const auto* Special = Cast<ASP1GravityCargo>(Item); Special && !Special->GravityZone) return false;
        if (Item->bTransferred || !Item->Definition || !Cargo.Contains(Item)
            || !Ledger.Credit(State.RunId, Item->CargoId, Item->Definition->Value)) return false;
        State.TeamValue = Ledger.TeamValue;
        Item->SetTransferred(true);
        LogEvent(TEXT("CargoTransferred"), Source, Item->CargoId, TEXT("Completed"), Item->Definition->Value);
        if (Cast<ASP1GravityCargo>(Item)) LogEvent(TEXT("GravityChange"),Source,Item->CargoId,TEXT("ZeroGravity; this car only"));
        GetOwner()->ForceNetUpdate();
        return true;
    }
    if (Source->Attempt.Target == MaintenanceStation && MaintenanceStation)
        return ReviveParticipants(Source);
    if (auto* Zone = Cast<ASP1ExtractionZone>(Source->Attempt.Target); CanUseExtraction(Zone)
        && Zone->ContainsPawn(Cast<APawn>(Source->GetOwner())))
    {
        // 한 서버 판정 경계에서 출발 장소를 먼저 고정한다. 다음 요청은 Phase/Id 검사에서 거부한다.
        Extraction = Zone;
        State.DepartureId = Zone->DepartureId;
        State.DepartureLabel = Zone->DepartureLabel;
        State.Phase = ESP1Phase::ExtractionCountdown;
        if (State.Maintenance.Phase == ESP1MaintenancePhase::Active) EndMaintenance(Now(),TEXT("DepartureStarted"));
        State.ExtractionDeadline = Now() + FMath::Clamp(DepartureSeconds, 0.1f, 120.0f);
        LogEvent(TEXT("ExtractionStart"), Source,Zone->DepartureId,Zone->DepartureLabel.ToString());
        GetOwner()->ForceNetUpdate();
        return true;
    }
    return false;
}

void USP1RoundComponent::Finish(ESP1Phase Result, const FString& Reason)
{
    if (!Ledger.Finalize(State.RunId, Result)) return;
    CloseCarVisits(TEXT("RunEnded"));
    Pings.Reset(); NextPingAt.Reset();
    EndMaintenance(Now(),TEXT("RoundEnded"));
    State.Phase = Result;
    State.FinalValue = Ledger.FinalValue;
    State.Reason = Reason;
    State.Escaped = 0;
    for (FSP1Participant& P : Participants)
    {
        APlayerController* PC = FindController(P.PlayerId);
        P.bEscaped = Result == ESP1Phase::Succeeded && P.bAlive && PC && Extraction && Extraction->ContainsPawn(PC->GetPawn());
        if (P.bEscaped) ++State.Escaped;
        if (PC && PC->GetPawn())
        {
            if (auto* Interaction = PC->GetPawn()->FindComponentByClass<USP1InteractionComponent>()) Interaction->CancelOnServer(TEXT("RoundEnded"));
            // 결과 이후 점프/저중력 추진도 끝낸다. 재시작은 새 Pawn의 기본 이동 모드를 사용한다.
            if (ACharacter* Character = Cast<ACharacter>(PC->GetPawn())) Character->GetCharacterMovement()->DisableMovement();
            PC->SetIgnoreMoveInput(true);
        }
    }
    State.LeftBehind = Participants.Num() - State.Escaped;
    LogEvent(TEXT("RunEnd"), nullptr, NAME_None, Reason, State.FinalValue);
    GetOwner()->ForceNetUpdate();
}

void USP1RoundComponent::LogEvent(const TCHAR* Event, const USP1InteractionComponent* Source,
    FName CargoId, const FString& Reason, int32 Value, FName ExplicitCar, int32 ExplicitPlayer) const
{
    if (!GetOwner()->HasAuthority()) return;
    TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
    // 스키마 2의 사건 이름은 계획서 10장과 맞추고 기존 이름은 추적용 필드로 보존한다.
    FString Type = Event;
    const bool bCargo = Source && Cast<ASP1TransferCargo>(Source->Attempt.Target);
    if (Type == TEXT("CargoTransferred")) Type = TEXT("AcquireComplete");
    else if (Type == TEXT("HoldStart")) Type = bCargo ? TEXT("AcquireStart") : TEXT("DeviceStart");
    else if (Type == TEXT("HoldCancel")) Type = bCargo ? TEXT("AcquireCancel") : TEXT("DeviceCancel");
    else if (Type == TEXT("Wounded")) Type = TEXT("HazardHit");
    else if (Type == TEXT("BulkheadOpened")) Type = TEXT("GateOpen");
    Row->SetStringField(TEXT("event"), Type);
    Row->SetStringField(TEXT("legacyEvent"), Event);
    Row->SetNumberField(TEXT("schemaVersion"),2);
    Row->SetNumberField(TEXT("sequence"),++LogSequence);
    Row->SetStringField(TEXT("buildId"),BuildIdentifier);
    Row->SetStringField(TEXT("settingsVersion"),SettingsVersion.ToString());
    Row->SetStringField(TEXT("layoutVariant"),LayoutVariant.ToString());
    Row->SetNumberField(TEXT("playerCount"),Participants.Num());
    Row->SetStringField(TEXT("RunId"), State.RunId.ToString());
    Row->SetStringField(TEXT("CargoId"), CargoId.ToString());
    FName CarId = NAME_None;
    if (const auto* Item = Source ? Cast<ASP1TransferCargo>(Source->Attempt.Target) : nullptr) CarId = Item->CarId;
    else if (const auto* Region = Source ? ASP1CarRegion::FindAt(this,Source->GetOwner()->GetActorLocation()) : nullptr) CarId = Region->CarId;
    else if (CargoId.ToString().StartsWith(TEXT("P01_C"))) CarId = FName(*CargoId.ToString().Left(7));
    if (!ExplicitCar.IsNone()) CarId = ExplicitCar;
    Row->SetStringField(TEXT("CarId"), CarId.ToString());
    Row->SetNumberField(TEXT("serverTime"), Now());
    Row->SetNumberField(TEXT("AttemptId"), Source ? Source->Attempt.AttemptId : 0);
    const APlayerController* PC = Source ? Source->GetPlayerController() : nullptr;
    Row->SetNumberField(TEXT("playerId"), ExplicitPlayer != INDEX_NONE ? ExplicitPlayer : PC && PC->PlayerState ? PC->PlayerState->GetPlayerId() : INDEX_NONE);
    Row->SetStringField(TEXT("reason"), Reason);
    Row->SetStringField(TEXT("phase"), StaticEnum<ESP1Phase>()->GetNameStringByValue(static_cast<int64>(State.Phase)));
    Row->SetNumberField(TEXT("value"), Value);
    Row->SetNumberField(TEXT("teamValue"), State.TeamValue);
    Row->SetStringField(TEXT("settingsId"), State.SettingsId.ToString());
    Row->SetNumberField(TEXT("missionLimit"), State.MissionLimit);
    Row->SetStringField(TEXT("departureId"), State.DepartureId.ToString());
    Row->SetNumberField(TEXT("escaped"), State.Escaped);
    Row->SetNumberField(TEXT("leftBehind"), State.LeftBehind);
    if (!CarId.IsNone()) Row->SetNumberField(TEXT("remainingCarValue"),RemainingCarValue(CarId));
    if (Type == TEXT("RunStart") || Type == TEXT("RunEnd"))
    {
        TSharedRef<FJsonObject> Remaining = MakeShared<FJsonObject>();
        for (const ASP1TransferCargo* Item : Cargo) if (Item) Remaining->SetNumberField(Item->CarId.ToString(),RemainingCarValue(Item->CarId));
        Row->SetObjectField(TEXT("remainingByCar"),Remaining);
    }
    FString Text;
    FJsonSerializer::Serialize(Row, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    UE_LOG(LogTemp, Display, TEXT("[SP1] %s"), *Text);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Prototype01/Runs");
    IFileManager::Get().MakeDirectory(*Directory, true);
    FFileHelper::SaveStringToFile(Text + LINE_TERMINATOR, *(Directory / (State.RunId.ToString() + TEXT(".jsonl"))),
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
}
