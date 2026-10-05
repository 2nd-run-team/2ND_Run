// 작성자 : 임진혁
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1ScenarioDefinition.h"
#include "Prototype01/SP1MaintenanceStation.h"
#include "Prototype01/SP1CarRegion.h"
#include "Prototype01/SP1CargoDefinition.h"
#include "Prototype01/SP1GravityCargo.h"
#include "Prototype01/SP1ExtractionZone.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void USP1RoundComponent::SelectRunSettings()
{
    int32 Connected = 0;
    for (const auto& P : Participants) Connected += P.bConnected;
    const USP1RunSettings* Settings = SettingsOverride ? SettingsOverride.Get()
        : Connected <= 2 ? TwoPlayerSettings.Get() : FourPlayerSettings.Get();
    State.SettingsId = bUseSettingsAssets && Settings ? Settings->SettingsId : FName(TEXT("DevelopmentOverride"));
    const float Seconds = bUseSettingsAssets && Settings ? Settings->MissionSeconds : MissionSeconds;
    State.MissionLimit = FMath::IsFinite(Seconds) ? FMath::Clamp(Seconds,1.f,3600.f) : 420.f;
}

void USP1RoundComponent::ValidateScenario()
{
    if (!Scenario) return; // 이전 소규모 fixture 맵에는 예산 데이터가 없다.
    TSet<FName> Regions;
    int32 RegionCount = 0;
    for (TActorIterator<ASP1CarRegion> It(GetWorld()); It; ++It) { Regions.Add(It->CarId); ++RegionCount; }
    bConfigurationValid &= RegionCount == 10 && Regions.Num() == 10 && Scenario->CarOrder.Num() == 10;
    TSet<FName> Ordered;
    for (FName Id : Scenario->CarOrder) { bConfigurationValid &= Regions.Contains(Id) && !Ordered.Contains(Id); Ordered.Add(Id); }
    int32 Normal = 0, Special = 0;
    int64 TotalValue = 0;
    TSet<const ASP1TransferCargo*> Matched;
    for (const auto& Row : Scenario->CargoBudget)
    {
        int32 Count = 0;
        bConfigurationValid &= Row.Definition && Row.Count > 0 && Regions.Contains(Row.CarId);
        for (const ASP1TransferCargo* Item : Cargo)
            if (Item->CarId == Row.CarId && Item->Definition == Row.Definition && !!Cast<ASP1GravityCargo>(Item) == Row.bGravityCargo)
            {
                bConfigurationValid &= !Matched.Contains(Item);
                Matched.Add(Item); ++Count;
            }
        bConfigurationValid &= Count == Row.Count;
    }
    for (const ASP1TransferCargo* Item : Cargo)
    {
        if (Cast<ASP1GravityCargo>(Item)) ++Special; else ++Normal;
        if (Item->Definition) TotalValue += Item->Definition->Value;
    }
    bConfigurationValid &= Matched.Num() == Cargo.Num() && Normal == Scenario->ExpectedNormal
        && Special == Scenario->ExpectedSpecial && TotalValue == Scenario->ExpectedValue;
    LogEvent(TEXT("ScenarioValidated"),nullptr,NAME_None,FString::Printf(TEXT("valid=%d normal=%d S01=%d cars=%d"),bConfigurationValid,Normal,Special,RegionCount),static_cast<int32>(TotalValue));
}

bool USP1RoundComponent::CanUseExtraction(const ASP1ExtractionZone* Zone) const
{
    // 클라이언트 표시에도 쓰므로 서버 전용 Exits 배열 외에 실제 레벨 Actor와 월드를 검사한다.
    if (!IsValid(Zone) || Zone->GetWorld() != GetWorld()) return false;
    if (GetOwner()->HasAuthority() && !Exits.Contains(Zone)) return false;
    return SP1::CanStartDeparture(State,Zone->bIntermediate,Now());
}
bool USP1RoundComponent::CanUseMaintenance() const
{
    return State.Phase == ESP1Phase::Running && State.Maintenance.Phase == ESP1MaintenancePhase::Active
        && Now() < State.Maintenance.Deadline && SP1::MissionRemaining(State,Now()) > 0;
}
bool USP1RoundComponent::IsHazardProtected(const APawn* Pawn) const
{
    for (TActorIterator<ASP1ExtractionZone> It(GetWorld()); It; ++It) if (It->ProtectsPawn(Pawn)) return true;
    return false;
}

void USP1RoundComponent::UpdateMaintenance(double Time)
{
    if (!MaintenanceStation || !MaintenanceStation->Region || State.Maintenance.Phase == ESP1MaintenancePhase::Closed) return;
    bool bEntered = false;
    for (const auto& P : Participants)
    {
        auto* PC = FindController(P.PlayerId);
        if (!P.bAlive || !PC || !PC->GetPawn()) continue;
        const auto* Region = ASP1CarRegion::FindAt(this,PC->GetPawn()->GetActorLocation());
        bEntered |= Region == MaintenanceStation->Region;
        // 먼저 앞칸으로 간 생존자가 있으면 뒤 팀원의 진입으로 새 휴식 창을 만들지 않는다.
        if (Region && Scenario && Scenario->CarOrder.Find(Region->CarId) >= 5) State.Maintenance.bForwardReached = true;
    }
    if (State.Maintenance.Phase == ESP1MaintenancePhase::Unused && bEntered && State.Phase == ESP1Phase::Running)
    {
        State.Maintenance.Phase = ESP1MaintenancePhase::Active;
        State.Maintenance.StartedAt = Time;
        State.Maintenance.Deadline = Time + FMath::Clamp(MaintenanceStation->WindowSeconds,1.f,30.f);
        State.Maintenance.FrozenMissionRemaining = FMath::Max(0.,State.MissionDeadline-Time);
        LogEvent(TEXT("MaintenanceStart"),nullptr,MaintenanceStation->StationId,TEXT("MissionClockOnly"));
        GetOwner()->ForceNetUpdate();
    }
    if (State.Maintenance.Phase != ESP1MaintenancePhase::Active) return;
    if (Time >= State.Maintenance.Deadline) EndMaintenance(State.Maintenance.Deadline,TEXT("WindowExpired"));
    else if (State.Maintenance.bForwardReached) EndMaintenance(Time,TEXT("SurvivorAdvanced"));
}

void USP1RoundComponent::EndMaintenance(double Time, FName Reason)
{
    if (!SP1::CloseMaintenance(State,Time,Reason)) return;
    // 화물·다른 장치 작업은 유지한다. 정비 패널과 아직 승인되지 않은 중간 출발만 취소한다.
    for (const auto& P : Participants)
        if (auto* PC = FindController(P.PlayerId); PC && PC->GetPawn())
            if (auto* Input = PC->GetPawn()->FindComponentByClass<USP1InteractionComponent>())
            {
                const auto* Exit = Cast<ASP1ExtractionZone>(Input->Attempt.Target);
                if (Input->Attempt.Target == MaintenanceStation || (Exit && Exit->bIntermediate && Exit != Extraction))
                    Input->CancelOnServer(Reason);
            }
    LogEvent(TEXT("MaintenanceEnd"),nullptr,MaintenanceStation ? MaintenanceStation->StationId : NAME_None,Reason.ToString());
    GetOwner()->ForceNetUpdate();
}

bool USP1RoundComponent::ReviveParticipants(USP1InteractionComponent* Source)
{
    if (!CanUseMaintenance() || !MaintenanceStation) return false;
    auto* GM = GetWorld()->GetAuthGameMode();
    if (!GM) return false;
    int32 Revived = 0;
    for (auto& P : Participants)
    {
        if (!SP1::CanRevive(P)) continue;
        auto* PC = FindController(P.PlayerId);
        APawn* OldPawn = PC ? PC->GetPawn() : nullptr;
        const auto* OldLife = OldPawn ? OldPawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
        if (!PC || !OldLife || !OldLife->IsDead()) continue;
        UClass* PawnClass = GM->GetDefaultPawnClassForController(PC);
        const auto* Template = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
        if (!Template) continue;
        const UCapsuleComponent* Capsule = Template->GetCapsuleComponent();
        APawn* NewPawn = nullptr;
        for (const FVector& Offset : MaintenanceStation->ReviveOffsets)
        {
            const FVector Position = MaintenanceStation->GetActorTransform().TransformPosition(Offset);
            FCollisionQueryParams Query(SCENE_QUERY_STAT(SP1Revive),false,OldPawn);
            if (!MaintenanceStation->Region->ContainsPoint(Position)
                || GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Pawn,
                    FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),Capsule->GetScaledCapsuleHalfHeight()),Query)) continue;
            NewPawn = GM->SpawnDefaultPawnAtTransform(PC,FTransform(FRotator::ZeroRotator,Position));
            if (NewPawn) break;
        }
        auto* Life = NewPawn ? NewPawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
        if (!Life) { if (NewPawn) NewPawn->Destroy(); continue; }
        // 새 Pawn은 아직 소유 클라이언트에게 보내지 않았다. 첫 이동/복제 전에 W/S를 초기화한다.
        Life->InitializeMaintenanceRevive();
        PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput();
        PC->Possess(NewPawn);
        if (PC->GetPawn() != NewPawn) { NewPawn->Destroy(); continue; }
        P.bMaintenanceRevived = true; P.bAlive = true;
        PC->SetControlRotation(FRotator::ZeroRotator); PC->ClientSetRotation(FRotator::ZeroRotator,true);
        PC->SetViewTarget(NewPawn); PC->ClientSetViewTarget(NewPawn);
        OldPawn->Destroy(); // 죽을 때 이미 떨어뜨린 소지품에는 접근하지 않는다.
        ++Revived;
        LogEvent(TEXT("Revive"),NewPawn->FindComponentByClass<USP1InteractionComponent>(),MaintenanceStation->StationId,
            FString::Printf(TEXT("player=%d W=50 S=50 maintenanceRevives=1"),P.PlayerId));
        NewPawn->ForceNetUpdate();
    }
    if (Revived == 0) LogEvent(TEXT("ReviveNoEligibleOrSafeSpawn"),Source,MaintenanceStation->StationId);
    GetOwner()->ForceNetUpdate();
    return Revived > 0;
}
