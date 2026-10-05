// 작성자 : 임진혁
#include "Prototype01/SP1SecurityCamera.h"
#include "Prototype01/SP1SessionSubsystem.h"
#include "Prototype01/SP1CarRegion.h"
#include "Prototype01/SP1DeviceRules.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
ASP1SecurityCamera::ASP1SecurityCamera()
{
    bReplicates = true; bAlwaysRelevant = true; SetNetUpdateFrequency(20);
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot")); Pivot->SetupAttachment(RootComponent);
    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head")); Head->SetupAttachment(Pivot);
    Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DirectionLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("DirectionLight")); DirectionLight->SetupAttachment(Pivot);
    DirectionLight->SetCastShadows(false); DirectionLight->SetIntensity(18000);
    Indicator = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Indicator")); Indicator->SetupAttachment(RootComponent);
    Indicator->SetRelativeLocation(FVector(0,0,65)); Indicator->SetWorldSize(18);
    Indicator->SetHorizontalAlignment(EHTA_Center);
}
void ASP1SecurityCamera::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASP1SecurityCamera, State); DOREPLIFETIME(ASP1SecurityCamera, Exposure);
}
FRotator ASP1SecurityCamera::GetAim(double Time) const
{
    const float Yaw = State.Phase == ESP1CameraPhase::Scanning
        ? GetActorRotation().Yaw + FMath::Sin((Time-State.Epoch)*2*PI/FMath::Max(.1f,SweepSeconds))*SweepHalfAngle : State.LockedYaw;
    return FRotator(AimPitch,Yaw,0);
}
bool ASP1SecurityCamera::CanSee(const APawn* Pawn, double Time) const
{
    const auto* Life = Pawn ? Pawn->FindComponentByClass<USP1SurvivalComponent>() : nullptr;
    // 범위 검사는 거리/시야보다 앞서며, 예고 시작과 타격 순간 모두 같은 검사를 사용한다.
    if (!Pawn || !Life || Life->IsDead() || !Region || !Region->ContainsPoint(Pawn->GetActorLocation())) return false;
    if (const auto* Round = USP1RoundComponent::Find(this); Round && Round->IsHazardProtected(Pawn)) return false;
    const FVector Origin = GetActorLocation();
    const FVector Point = Pawn->GetActorLocation() + FVector(0,0,30);
    const FVector ToPawn = Point-Origin;
    if (ToPawn.SizeSquared() > FMath::Square(FMath::Max(0.f,Range))
        || FVector::DotProduct(GetAim(Time).Vector(),ToPawn.GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(FullAngle,1.f,179.f)*.5f))) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SP1Camera),false,this);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Origin,Point,ECC_Visibility,Query) || Hit.GetActor() == Pawn;
}
void ASP1SecurityCamera::ResetForRun(FGuid RunId, double Time)
{
    if (!HasAuthority()) return;
    State = FSP1CameraState(); State.RunId = RunId; State.Epoch = Time;
    Exposure.Reset(); ForceNetUpdate();
}
void ASP1SecurityCamera::EvaluateOnServer(double Time, float Delta)
{
    auto* Round = USP1RoundComponent::Find(this);
    if (!HasAuthority() || !Round || State.RunId != Round->State.RunId || !SP1::IsPlaying(Round->State.Phase)) return;
    if (State.Phase == ESP1CameraPhase::Cooldown)
    {
        if (Time < State.Deadline) return;
        State.Phase = ESP1CameraPhase::Scanning; ForceNetUpdate();
    }
    if (State.Phase == ESP1CameraPhase::Warning)
    {
        if (Time < State.Deadline) return;
        int32 Hits = 0;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (auto* PC = It->Get(); PC && Round->CanInteract(PC->GetPawn()) && CanSee(PC->GetPawn(),Time))
            { Round->QueueDamage(PC->GetPawn()->FindComponentByClass<USP1SurvivalComponent>(),WoundAmount,HazardId); ++Hits; }
        Round->LogEvent(TEXT("CameraPulse"),nullptr,HazardId,FString::Printf(TEXT("VisibleInCar=%d"),Hits));
        State.Phase = ESP1CameraPhase::Cooldown; State.Deadline = Time + FMath::Max(.1f,CooldownSeconds);
        Exposure.Reset(); ForceNetUpdate(); return;
    }
    bool bDetected = false;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* PC = It->Get();
        if (!PC || !PC->PlayerState || !Round->CanInteract(PC->GetPawn())) continue;
        FSP1Exposure* Row = Exposure.FindByPredicate([PC](const FSP1Exposure& Item){return Item.Player == PC->PlayerState;});
        if (!Row) { FSP1Exposure New; New.Player = PC->PlayerState; Row = &Exposure.Add_GetRef(New); }
        Row->Amount = SP1::UpdateExposure(Row->Amount,CanSee(PC->GetPawn(),Time),Delta,ExposureSeconds,DecayPerSecond);
        bDetected |= Row->Amount >= 1;
    }
    if (bDetected)
    {
        State.LockedYaw = GetAim(Time).Yaw; State.Phase = ESP1CameraPhase::Warning;
        State.Deadline = Time + FMath::Max(.1f,WarningSeconds);
        Round->LogEvent(TEXT("CameraWarning"),nullptr,HazardId,TEXT("CoverOrLeaveCar")); ForceNetUpdate();
    }
}
void ASP1SecurityCamera::Tick(float Delta)
{
    Super::Tick(Delta);
    const auto* Round = USP1RoundComponent::Find(this);
    if (!Round) return;
    const bool bPlaying = SP1::IsPlaying(Round->State.Phase) && State.RunId == Round->State.RunId;
    const bool bWarning = bPlaying && State.Phase == ESP1CameraPhase::Warning;
    // 복제 상태 전환에만 짧은 경고음을 재생한다. 타격/종료/월드 해제 뒤 소리가 남지 않는다.
    if (bWarning != bWasWarning)
    {
        if (IsValid(WarningAudio)) WarningAudio->Stop();
        if (bWarning && WarningSound && GetNetMode()!=NM_DedicatedServer && USP1SessionSubsystem::CanPlayAudio(this))
        {
            WarningAudio = UGameplayStatics::SpawnSoundAtLocation(this,WarningSound,GetActorLocation(),FRotator::ZeroRotator,.3f,1,0,Attenuation);
            USP1SessionSubsystem::TrackAudio(this,WarningAudio);
        }
        bWasWarning = bWarning;
    }
    Pivot->SetWorldRotation(GetAim(Round->Now()));
    DirectionLight->SetVisibility(bPlaying && State.Phase != ESP1CameraPhase::Cooldown);
    DirectionLight->SetAttenuationRadius(Range); DirectionLight->SetOuterConeAngle(FullAngle*.5f); DirectionLight->SetInnerConeAngle(FullAngle*.35f);
    DirectionLight->SetLightColor(State.Phase == ESP1CameraPhase::Warning ? FLinearColor::Red : FLinearColor(1,.5f,.03f));
    const FString Label = !bPlaying ? TEXT("CAMERA / READY") : State.Phase == ESP1CameraPhase::Warning
        ? FString::Printf(TEXT("TAKE COVER %.1fs"),FMath::Max(0.,State.Deadline-Round->Now()))
        : State.Phase == ESP1CameraPhase::Cooldown ? TEXT("COOLDOWN / CROSS NOW") : TEXT("SCANNING / 60 DEG");
    Indicator->SetText(FText::FromString(Label));
    Indicator->SetTextRenderColor(State.Phase == ESP1CameraPhase::Warning ? FColor::Red : FColor::Yellow);
}
void ASP1SecurityCamera::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(WarningAudio)) WarningAudio->Stop();
    Super::EndPlay(Reason);
}
