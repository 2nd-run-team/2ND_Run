#include "SPLaserSecurityDevice.h"

#include "SPGuardAlertSubsystem.h"
#include "SPStealthGameStateComponent.h"
#include "SPPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogSPLaser, Log, All);

ASPLaserSecurityDevice::ASPLaserSecurityDevice()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
    SetNetUpdateFrequency(20);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    DetectionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionVolume"));
    DetectionVolume->SetupAttachment(SceneRoot);
    DetectionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    DetectionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    DetectionVolume->SetGenerateOverlapEvents(false);
    DetectionVolume->SetCanEverAffectNavigation(false);
    DetectionVolume->SetHiddenInGame(true);
    EmitterVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EmitterVisual"));
    ReceiverVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReceiverVisual"));
    BeamVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamVisual"));
    for (auto* Mesh : {EmitterVisual.Get(), ReceiverVisual.Get(), BeamVisual.Get()})
    {
        Mesh->SetupAttachment(SceneRoot);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetCastShadow(false);
    }
    StateIndicator = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StateIndicator"));
    StateIndicator->SetupAttachment(SceneRoot);
    StateIndicator->SetHorizontalAlignment(EHTA_Center);
    StateIndicator->SetWorldSize(18);
    StateIndicator->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASPLaserSecurityDevice::BeginPlay()
{
    Super::BeginPlay();
    UpdateGeometry();
    if (HasAuthority()) ResetDevice();
    UpdatePresentation(false);
}

void ASPLaserSecurityDevice::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    UpdateGeometry();
    if (!GetWorld() || !GetWorld()->IsGameWorld()) Phase = GetPhaseAtElapsed(0);
    UpdatePresentation(false);
}

FVector ASPLaserSecurityDevice::GetBeamStart() const { return GetActorTransform().TransformPosition(LocalStart); }
FVector ASPLaserSecurityDevice::GetBeamEnd() const
{
    const FVector Start = GetBeamStart();
    const FVector End = IsValid(ReceiverActor) ? ReceiverActor->GetActorLocation() : GetActorTransform().TransformPosition(LocalEnd);
    return bUseBeamLength ? Start + (End - Start).GetSafeNormal(SMALL_NUMBER, GetActorForwardVector()) * FMath::Max(1.f, BeamLength) : End;
}

ESPLaserPhase ASPLaserSecurityDevice::GetPhaseAtElapsed(double Elapsed) const
{
    if (!bEnabled) return ESPLaserPhase::Off;
    if (Mode == ESPLaserMode::AlwaysOn) return ESPLaserPhase::On;
    const double On = FMath::Max(.1, double(OnSeconds));
    const double Off = FMath::Max(.1, double(OffSeconds));
    double T = FMath::Fmod(Elapsed + InitialPhaseSeconds, On + Off);
    if (T < 0) T += On + Off;
    if (T < On) return T >= On - FMath::Clamp(double(WarningSeconds), 0., On) ? ESPLaserPhase::WarningOff : ESPLaserPhase::On;
    return T >= On + Off - FMath::Clamp(double(WarningSeconds), 0., Off) ? ESPLaserPhase::WarningOn : ESPLaserPhase::Off;
}

bool ASPLaserSecurityDevice::IsBeamActive() const { return Phase == ESPLaserPhase::On || Phase == ESPLaserPhase::WarningOff; }

void ASPLaserSecurityDevice::UpdateGeometry()
{
    const FVector Start = GetBeamStart(), End = GetBeamEnd();
    const FVector Direction = (End - Start).GetSafeNormal(SMALL_NUMBER, GetActorForwardVector());
    const FQuat Rotation = Direction.ToOrientationQuat();
    DetectionVolume->SetWorldLocationAndRotation((Start + End) * .5, Rotation);
    DetectionVolume->SetWorldScale3D(FVector::OneVector);
    DetectionVolume->SetBoxExtent(FVector(FMath::Max(.05, FVector::Distance(Start, End) * .5),
        FMath::Max(.05f, DetectionThickness * .5f), FMath::Max(.05f, DetectionThickness * .5f)), false);
    EmitterVisual->SetWorldLocation(Start);
    ReceiverVisual->SetWorldLocation(End);
    BeamVisual->SetWorldLocationAndRotation((Start + End) * .5, Rotation);
    // 교체 메시의 원래 크기를 표시 크기에 맞춘다. 실제 판정은 DetectionVolume의 Overlap/Sweep만 사용한다.
    FVector NativeSize(100);
    if (BeamVisual->GetStaticMesh()) NativeSize = BeamVisual->GetStaticMesh()->GetBounds().BoxExtent * 2;
    BeamVisual->SetWorldScale3D(FVector(FMath::Max(.1, FVector::Distance(Start, End)) / FMath::Max(.01, NativeSize.X),
        FMath::Max(.1f, VisualThickness) / FMath::Max(.01, NativeSize.Y), FMath::Max(.1f, VisualThickness) / FMath::Max(.01, NativeSize.Z)));
    for (auto* Mesh : {EmitterVisual.Get(), ReceiverVisual.Get(), BeamVisual.Get()})
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetCanEverAffectNavigation(false);
    }
    StateIndicator->SetWorldLocation(Start + GetActorUpVector() * 60);
}

void ASPLaserSecurityDevice::ClearSamples()
{
    Samples.Reset(); Pending.Reset(); PendingCount = 0;
    PreviousSampleTime = -1; NextCallTime = 0; bPreviousActive = false;
}

void ASPLaserSecurityDevice::ResetDevice()
{
    if (!HasAuthority() || !GetWorld()) return;
    ClearSamples();
    EpochServerTime = GetWorld()->GetTimeSeconds();
    auto* S = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    auto* State = S ? S->GetSecurityState() : nullptr;
    StageId = State ? State->GetAlarmState().StageId : FGuid();
    bStageWasActive = S && S->IsStageActive();
    ContactCount = ReentryCount = FastCrossCount = ActivationCount = ContinuousCount = 0;
    IncidentCount = SubmissionCount = DroppedContactCount = 0;
    LastContactType = ESPLaserContactType::Entry;
    LastIncident = FSPStealthIncidentRecord();
    Phase = bStageWasActive ? GetPhaseAtElapsed(0) : ESPLaserPhase::Off;
    UpdateGeometry(); UpdatePresentation(false); ForceNetUpdate();
}

void ASPLaserSecurityDevice::RefreshDevice()
{
    if (GetWorld() && GetWorld()->IsGameWorld() && !HasAuthority()) return;
    ClearSamples(); UpdateGeometry();
    if (GetWorld()) Phase = bStageWasActive ? GetPhaseAtElapsed(GetWorld()->GetTimeSeconds() - EpochServerTime) : ESPLaserPhase::Off;
    UpdatePresentation(false); ForceNetUpdate();
}

void ASPLaserSecurityDevice::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FVector Start = GetBeamStart(), End = GetBeamEnd();
    const bool bChanged = !Start.Equals(PreviousBeamStart, .01) || !End.Equals(PreviousBeamEnd, .01);
    if (bChanged) UpdateGeometry();
    PreviousBeamStart = Start; PreviousBeamEnd = End;
    if (!GetWorld()->IsGameWorld())
    {
        Phase = GetPhaseAtElapsed(0); UpdatePresentation(false); DrawDetection(); return;
    }
    if (HasAuthority())
    {
        auto* S = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
        auto* Security = S ? S->GetSecurityState() : nullptr;
        const FGuid CurrentStage = Security ? Security->GetAlarmState().StageId : FGuid();
        const bool bActiveStage = S && S->IsStageActive();
        if (CurrentStage != StageId || (bActiveStage && !bStageWasActive)) ResetDevice();
        if (!bActiveStage)
        {
            if (bStageWasActive) ClearSamples();
            bStageWasActive = false;
            if (Phase != ESPLaserPhase::Off) { Phase = ESPLaserPhase::Off; ForceNetUpdate(); }
        }
        else
        {
            bStageWasActive = true;
            const double Now = GetWorld()->GetTimeSeconds();
            const ESPLaserPhase NewPhase = GetPhaseAtElapsed(Now - EpochServerTime);
            if (NewPhase != Phase) { Phase = NewPhase; ForceNetUpdate(); }
            // 서버가 상태 전환과 접촉을 함께 판정한다. 참가자는 복제된 Phase와 사건 횟수로 표시만 갱신한다.
            SamplePlayers(Now, bChanged);
            SubmitDueContact(Now);
        }
    }
    UpdatePresentation(true);
    if (bDrawDetection) DrawDetection();
}

bool ASPLaserSecurityDevice::SweepActiveInterval(const FPlayerSample& Previous, const FPlayerSample& Current, double From, double To) const
{
    if (!bEnabled || To <= From || !DetectionVolume->GetBodyInstance()) return false;
    const double On = FMath::Max(.1, double(OnSeconds)), Cycle = On + FMath::Max(.1, double(OffSeconds));
    double Cursor = From;
    // 프레임 사이 이동을 켜짐/꺼짐 경계로 나누고 켜져 있던 구간만 Sweep한다.
    // 현재 위치의 Overlap만 보면 한 프레임에 얇은 빔을 지나친 플레이어를 놓칠 수 있다.
    while (Cursor < To - UE_SMALL_NUMBER)
    {
        double PhaseTime = FMath::Fmod(Cursor - EpochServerTime + InitialPhaseSeconds, Cycle);
        if (PhaseTime < 0) PhaseTime += Cycle;
        const bool bOn = Mode == ESPLaserMode::AlwaysOn || PhaseTime < On;
        const double Boundary = Mode == ESPLaserMode::AlwaysOn ? To : Cursor + (bOn ? On - PhaseTime : Cycle - PhaseTime);
        const double SegmentEnd = FMath::Min(To, Boundary);
        if (bOn)
        {
            const double A = (Cursor - From) / (To - From), B = (SegmentEnd - From) / (To - From);
            // 이동은 연속 캡슐 Sweep으로 검사한다. 중력에 의한 회전은 각도를 나눠 검사하고
            // 중간 자세의 누락을 줄이도록 반지름 여유를 더한다. 경계에서는 보수적으로 접촉할 수 있다.
            const float Angle = Previous.Rotation.AngularDistance(Current.Rotation) * float(B - A);
            const int32 Steps = FMath::Max(1, FMath::CeilToInt(Angle / FMath::DegreesToRadians(10.f)));
            for (int32 I = 0; I < Steps; ++I)
            {
                const double L = FMath::Lerp(A, B, double(I) / Steps), R = FMath::Lerp(A, B, double(I + 1) / Steps);
                const FQuat Rotation = FQuat::Slerp(Previous.Rotation, Current.Rotation, float((L + R) * .5));
                const float Radius = FMath::Max(Previous.Radius, Current.Radius);
                const float Height = FMath::Max(Previous.HalfHeight, Current.HalfHeight);
                const float Margin = (Height - Radius) * FMath::Sin(Angle / (2 * Steps));
                FHitResult Hit;
                if (DetectionVolume->GetBodyInstance()->Sweep(Hit, FMath::Lerp(Previous.Location, Current.Location, L),
                    FMath::Lerp(Previous.Location, Current.Location, R), Rotation,
                    FCollisionShape::MakeCapsule(Radius + Margin, Height + Margin), false)) return true;
            }
        }
        // 부동소수점 나머지 오차로 동일한 전환 경계에서 반복하지 않도록 조금 전진한다.
        Cursor = SegmentEnd + 1.e-7;
    }
    return false;
}

void ASPLaserSecurityDevice::QueueContact(FPlayerSample& Sample, ESPLaserContactType Type)
{
    auto* S = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    // 경비가 갈 곳은 접촉자의 현재 위치가 아닌 사건 당시 장치 위치다. 새 진입마다 새 사건 ID를 만든다.
    Sample.Episode = S->MakeIncidentContext(GetActorLocation(), DispatchGroup);
    Sample.Episode.ResponseRadius = ResponseRadius;
    Sample.Episode.MaxResponders = MaxResponders;
    Sample.bEverTouched = true;
    ++ContactCount; LastContactType = Type;
    if (Type == ESPLaserContactType::Reentry) ++ReentryCount;
    if (Type == ESPLaserContactType::FastCross) ++FastCrossCount;
    if (Type == ESPLaserContactType::ActivatedInside) ++ActivationCount;
    if (Pending.Num() < 128) Pending.Add({Sample.Episode, Type});
    else
    {
        ++DroppedContactCount;
        UE_LOG(LogSPLaser, Warning, TEXT("Device=%s pending contact limit reached (128), dropped=%d"), *DeviceId.ToString(), DroppedContactCount);
    }
    PendingCount = Pending.Num();
    ForceNetUpdate();
}

void ASPLaserSecurityDevice::SamplePlayers(double Now, bool bGeometryChanged)
{
    TSet<TWeakObjectPtr<ASPPlayerCharacter>> Present;
    const bool bOn = IsBeamActive();
    const double Cycle = FMath::Max(.1, double(OnSeconds)) + FMath::Max(.1, double(OffSeconds));
    const bool bPassedActivation = Mode == ESPLaserMode::Periodic && PreviousSampleTime >= 0 &&
        FMath::FloorToDouble((Now - EpochServerTime + InitialPhaseSeconds) / Cycle) >
        FMath::FloorToDouble((PreviousSampleTime - EpochServerTime + InitialPhaseSeconds) / Cycle);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* P = It->Get() ? Cast<ASPPlayerCharacter>(It->Get()->GetPawn()) : nullptr;
        if (!IsValid(P) || !P->IsPlayerControlled() || !P->GetCapsuleComponent()) continue;
        Present.Add(P);
        auto* Capsule = P->GetCapsuleComponent();
        FPlayerSample Current;
        Current.Location = Capsule->GetComponentLocation(); Current.Rotation = Capsule->GetComponentQuat();
        Capsule->GetScaledCapsuleSize(Current.Radius, Current.HalfHeight);
        FPlayerSample* Previous = Samples.Find(P);
        if (Previous) { Current.bEverTouched = Previous->bEverTouched; Current.Episode = Previous->Episode; }
        const bool bOverlap = DetectionVolume->GetBodyInstance() && DetectionVolume->GetBodyInstance()->OverlapTest(
            Current.Location, Current.Rotation, FCollisionShape::MakeCapsule(Current.Radius, Current.HalfHeight));
        const bool bInside = bOn && bOverlap;
        const bool bSwept = Previous && !bGeometryChanged && PreviousSampleTime >= 0 &&
            SweepActiveInterval(*Previous, Current, PreviousSampleTime, Now);
        const bool bActivatedInside = bInside && (!Previous || !bPreviousActive || bGeometryChanged || bPassedActivation);
        // 초기화 직후에는 현재 겹침만 검사한다. 이전 표본이 없는데 과거 통과 경로를 만들어내지 않는다.
        if ((bInside || bSwept) && (!Previous || !Previous->bInside || !bPreviousActive || bGeometryChanged || bActivatedInside))
        {
            const ESPLaserContactType Type = bActivatedInside
                ? ESPLaserContactType::ActivatedInside : (!bInside ? ESPLaserContactType::FastCross :
                    (Current.bEverTouched ? ESPLaserContactType::Reentry : ESPLaserContactType::Entry));
            QueueContact(Current, Type);
        }
        Current.bInside = bInside;
        Samples.Add(P, Current);
    }
    for (auto It = Samples.CreateIterator(); It; ++It) if (!Present.Contains(It.Key())) It.RemoveCurrent();
    PreviousSampleTime = Now; bPreviousActive = bOn;
}

void ASPLaserSecurityDevice::SubmitDueContact(double Now)
{
    if (Now < NextCallTime) return;
    FPendingContact Contact;
    bool bFound = false;
    if (!Pending.IsEmpty())
    {
        Contact = Pending[0]; Pending.RemoveAt(0); PendingCount = Pending.Num(); bFound = true;
    }
    else
    {
        // 계속 닿아 있으면 같은 사건 ID로 재호출한다. Subsystem의 중복 제거가 추가 출동을 막는다.
        for (const auto& Pair : Samples) if (Pair.Value.bInside && Pair.Value.Episode.IncidentId.IsValid())
        {
            Contact = {Pair.Value.Episode, ESPLaserContactType::Continuous}; bFound = true; ++ContinuousCount; break;
        }
    }
    if (!bFound) return;
    // 호출 간격은 장치 전체에 적용한다. 재진입은 새 사건으로 대기하며, 다른 센서의 간격과 섞이지 않는다.
    NextCallTime = Now + FMath::Max(.01f, CallInterval);
    LastContactType = Contact.Type;
    LastIncident = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>()->SubmitAnonymousIncident(ESPStealthIncident::LaserContact, Contact.Context);
    ++SubmissionCount;
    if (LastIncident.Result == ESPStealthResult::Applied) ++IncidentCount;
    UE_LOG(LogSPLaser, Log, TEXT("Device=%s Contact=%s Incident=%s Stage=%s Location=%s ServerTime=%.3f Result=%s Calls=%d"),
        *DeviceId.ToString(), *UEnum::GetValueAsString(Contact.Type), *Contact.Context.IncidentId.ToString(), *Contact.Context.StageId.ToString(),
        *Contact.Context.Location.ToString(), LastIncident.ServerTime, *UEnum::GetValueAsString(LastIncident.Result), SubmissionCount);
    ForceNetUpdate();
}

void ASPLaserSecurityDevice::UpdatePresentation(bool bAllowSound)
{
    const bool bWarning = Phase == ESPLaserPhase::WarningOn || Phase == ESPLaserPhase::WarningOff;
    UMaterialInterface* Material = bWarning ? WarningMaterial : (IsBeamActive() ? OnMaterial : OffMaterial);
    if (Material) for (auto* Mesh : {EmitterVisual.Get(), ReceiverVisual.Get(), BeamVisual.Get()}) Mesh->SetMaterial(0, Material);
    const FString State = Phase == ESPLaserPhase::On ? TEXT("켜짐 · 접촉 위험") : Phase == ESPLaserPhase::WarningOff ? TEXT("곧 꺼짐 · 아직 위험") :
        Phase == ESPLaserPhase::WarningOn ? TEXT("곧 켜짐 · 통과 주의") : TEXT("꺼짐 · 통과 가능");
    StateIndicator->SetText(FText::FromString(TEXT("보안 레이저\n") + State));
    StateIndicator->SetTextRenderColor(bWarning ? FColor::Yellow : (IsBeamActive() ? FColor::Red : FColor::Green));
    if (bAllowSound && bPresentationInitialized && GetNetMode() != NM_DedicatedServer)
    {
        if (bWarning && PresentedPhase != Phase && WarningSound) UGameplayStatics::PlaySoundAtLocation(this, WarningSound, GetBeamStart());
        if (IncidentCount > PresentedIncidentCount && ContactSound) UGameplayStatics::PlaySoundAtLocation(this, ContactSound, GetBeamStart());
    }
    PresentedPhase = Phase; PresentedIncidentCount = IncidentCount; bPresentationInitialized = true;
}

void ASPLaserSecurityDevice::DrawDetection() const
{
#if !UE_BUILD_SHIPPING
    DrawDebugBox(GetWorld(), DetectionVolume->GetComponentLocation(), DetectionVolume->GetUnscaledBoxExtent(),
        DetectionVolume->GetComponentQuat(), IsBeamActive() ? FColor::Red : FColor::Green, false, 0, 0, 1.5);
    DrawDebugSphere(GetWorld(), GetBeamStart(), 10, 8, FColor::Cyan, false, 0);
    DrawDebugSphere(GetWorld(), GetBeamEnd(), 10, 8, FColor::Cyan, false, 0);
#endif
}

void ASPLaserSecurityDevice::OnRep_Config() { UpdateGeometry(); UpdatePresentation(false); }
void ASPLaserSecurityDevice::OnRep_Phase() { UpdatePresentation(true); }
void ASPLaserSecurityDevice::OnRep_IncidentCount() { UpdatePresentation(true); }

void ASPLaserSecurityDevice::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPLaserSecurityDevice, LocalStart); DOREPLIFETIME(ASPLaserSecurityDevice, LocalEnd);
    DOREPLIFETIME(ASPLaserSecurityDevice, ReceiverActor); DOREPLIFETIME(ASPLaserSecurityDevice, bUseBeamLength);
    DOREPLIFETIME(ASPLaserSecurityDevice, BeamLength); DOREPLIFETIME(ASPLaserSecurityDevice, DetectionThickness);
    DOREPLIFETIME(ASPLaserSecurityDevice, Mode); DOREPLIFETIME(ASPLaserSecurityDevice, OnSeconds); DOREPLIFETIME(ASPLaserSecurityDevice, OffSeconds);
    DOREPLIFETIME(ASPLaserSecurityDevice, InitialPhaseSeconds); DOREPLIFETIME(ASPLaserSecurityDevice, WarningSeconds);
    DOREPLIFETIME(ASPLaserSecurityDevice, bEnabled); DOREPLIFETIME(ASPLaserSecurityDevice, DeviceId); DOREPLIFETIME(ASPLaserSecurityDevice, DispatchGroup);
    DOREPLIFETIME(ASPLaserSecurityDevice, Phase); DOREPLIFETIME(ASPLaserSecurityDevice, EpochServerTime); DOREPLIFETIME(ASPLaserSecurityDevice, StageId);
    DOREPLIFETIME(ASPLaserSecurityDevice, ContactCount); DOREPLIFETIME(ASPLaserSecurityDevice, ReentryCount); DOREPLIFETIME(ASPLaserSecurityDevice, FastCrossCount);
    DOREPLIFETIME(ASPLaserSecurityDevice, ActivationCount); DOREPLIFETIME(ASPLaserSecurityDevice, ContinuousCount);
    DOREPLIFETIME(ASPLaserSecurityDevice, IncidentCount); DOREPLIFETIME(ASPLaserSecurityDevice, SubmissionCount); DOREPLIFETIME(ASPLaserSecurityDevice, PendingCount);
    DOREPLIFETIME(ASPLaserSecurityDevice, DroppedContactCount); DOREPLIFETIME(ASPLaserSecurityDevice, LastContactType); DOREPLIFETIME(ASPLaserSecurityDevice, LastIncident);
}
