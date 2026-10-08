#include "SPStealthTestConsole.h"
#include "SPStealthStatusWidget.h"
#include "SPGuardAlertSubsystem.h"
#include "SPInteractableComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
const TCHAR* IncidentLabel(ESPStealthIncident Kind)
{
    switch (Kind)
    {
    case ESPStealthIncident::SustainedCrimeConfirmed: return TEXT("지속 범죄 목격");
    case ESPStealthIncident::InstantCrimeWitnessed: return TEXT("순간 범죄 목격");
    case ESPStealthIncident::DirectReportCompleted: return TEXT("직접 목격 신고");
    case ESPStealthIncident::LaserContact: return TEXT("레이저 접촉");
    case ESPStealthIncident::WorkNoise: return TEXT("작업 소리");
    case ESPStealthIncident::IndirectReport: return TEXT("간접 신고");
    case ESPStealthIncident::VictimReport: return TEXT("피해자 신고");
    case ESPStealthIncident::IdentifiedPlayerRediscovered: return TEXT("발각자 재발견");
    case ESPStealthIncident::EscapeActivated: return TEXT("탈출 장치 작동");
    default: return TEXT("알 수 없는 사건");
    }
}
const TCHAR* ResultLabel(ESPStealthResult Result)
{
    switch (Result)
    {
    case ESPStealthResult::Applied: return TEXT("처리 완료");
    case ESPStealthResult::Duplicate: return TEXT("중복 사건 · 추가 처리 없음");
    case ESPStealthResult::NotAuthority: return TEXT("서버 권한 없음");
    case ESPStealthResult::InactiveStage: return TEXT("시험 종료 상태");
    case ESPStealthResult::StaleStage: return TEXT("이전 회차 사건 · 무시됨");
    case ESPStealthResult::InvalidRequest: return TEXT("유효하지 않은 요청");
    case ESPStealthResult::UnknownIdentity: return TEXT("아직 미발각 · 재발견 불가");
    default: return TEXT("알 수 없는 결과");
    }
}
}

ASPStealthTestDirector::ASPStealthTestDirector()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    StatusWidgetClass = USPStealthStatusWidget::StaticClass();
}
void ASPStealthTestDirector::BeginPlay()
{
    Super::BeginPlay();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (HasAuthority())
    {
        auto* S = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
        S->OnFirstPlayerIdentifiedNative.AddUObject(this, &ThisClass::OnIdentity);
        S->OnFirstGlobalAlarmNative.AddUObject(this, &ThisClass::OnAlarm);
    }
#endif
}
void ASPStealthTestDirector::OnIdentity(const FSPStealthIncidentRecord&) { ++FirstIdentityEffects; ForceNetUpdate(); }
void ASPStealthTestDirector::OnAlarm(const FSPStealthIncidentRecord&) { ++FirstAlarmEffects; ForceNetUpdate(); }
void ASPStealthTestDirector::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (GetNetMode() == NM_DedicatedServer || !StatusWidgetClass) { return; }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        auto* PC = It->Get();
        if (!PC || !PC->IsLocalController()) { continue; }
        if (Widgets.ContainsByPredicate([PC](const UUserWidget* W) { return W && W->GetOwningPlayer() == PC; })) { continue; }
        if (auto* Widget = CreateWidget<UUserWidget>(PC, StatusWidgetClass))
        {
            Widget->AddToPlayerScreen(20);
            Widget->SetPositionInViewport(FVector2D(16, 80));
            Widget->SetDesiredSizeInViewport(FVector2D(480, 280));
            Widgets.Add(Widget);
        }
    }
#endif
}
void ASPStealthTestDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    for (UUserWidget* Widget : Widgets) { if (Widget) { Widget->RemoveFromParent(); } }
    if (auto* S = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>())
    {
        S->OnFirstPlayerIdentifiedNative.RemoveAll(this);
        S->OnFirstGlobalAlarmNative.RemoveAll(this);
    }
    Super::EndPlay(Reason);
}
void ASPStealthTestDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPStealthTestDirector, FirstIdentityEffects);
    DOREPLIFETIME(ASPStealthTestDirector, FirstAlarmEffects);
    DOREPLIFETIME(ASPStealthTestDirector, LastResult);
}
void ASPStealthTestDirector::Execute(ESPStealthTestCommand Command, ESPStealthIncident Kind,
    APawn* User, FVector Location, FName Scope, FName Group, float ResponseRadius, int32 MaxResponders)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (!HasAuthority() || !IsValid(User) || !User->IsPlayerControlled()) { return; }
    auto* S = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();
    if (Command == ESPStealthTestCommand::StartStage || Command == ESPStealthTestCommand::RestartStage
        || Command == ESPStealthTestCommand::EndStage)
    {
        for (TActorIterator<ASPStealthTestConsole> It(GetWorld()); It; ++It)
        {
            if (auto* CurrentUser = It->Interactable->GetCurrentUser()) { It->Interactable->CancelBy(CurrentUser); }
        }
        if (Command == ESPStealthTestCommand::EndStage) { S->EndStage(); }
        else if (Command == ESPStealthTestCommand::StartStage) { S->StartStage(); }
        else { S->RestartStage(); }
        FirstIdentityEffects = FirstAlarmEffects = 0;
        LastResult = Command == ESPStealthTestCommand::EndStage ? TEXT("시험 종료 · 보안 감지 중지") :
            Command == ESPStealthTestCommand::StartStage ? TEXT("새 시험 시작 · 보안 상태 초기화") :
            TEXT("보안 초기화 완료 · 물건과 위치는 유지");
    }
    else if (Command==ESPStealthTestCommand::ObservedCrime)
    {
        LastResult=TEXT("범죄 행동 완료 · 실제 목격 여부로 발각 판정");
    }
    else
    {
        auto Context = S->MakeIncidentContext(Location, Group);
        Context.ResponseRadius = ResponseRadius;
        Context.MaxResponders = MaxResponders;
        auto* PS = User->GetPlayerState();
        if (Command == ESPStealthTestCommand::ReplayLastIncident)
        {
            Context = LastTestIncident.Context;
            Kind = LastTestIncident.Kind;
            Scope = LastTestIncident.IdentityScope;
            PS = LastTestIncident.Player.Get();
        }
        const bool bDirect = Kind == ESPStealthIncident::SustainedCrimeConfirmed || Kind == ESPStealthIncident::InstantCrimeWitnessed
            || Kind == ESPStealthIncident::DirectReportCompleted || Kind == ESPStealthIncident::IdentifiedPlayerRediscovered;
        const auto Result = bDirect ? S->SubmitDirectIncident(Kind, PS, Scope, Context)
            : Kind==ESPStealthIncident::WorkNoise ? S->ReportWorkNoise(Context) : S->SubmitAnonymousIncident(Kind, Context);
        if (Command == ESPStealthTestCommand::SubmitIncident) { LastTestIncident = Result; }
        LastResult = FString::Printf(TEXT("%s: %s\n사건 번호 %s | 출동 %d명"),
            IncidentLabel(Kind), ResultLabel(Result.Result), *Context.IncidentId.ToString().Left(8), Result.DispatchedGuards);
    }
    ForceNetUpdate();
#endif
}
ASPStealthTestConsole::ASPStealthTestConsole()
{
    bReplicates = true;
    InteractionVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionVolume"));
    SetRootComponent(InteractionVolume);
    InteractionVolume->SetBoxExtent(FVector(35, 45, 45));
    InteractionVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionVolume->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
    Visual->SetupAttachment(InteractionVolume);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCanEverAffectNavigation(false);
    Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
    Label->SetupAttachment(InteractionVolume);
    Label->SetRelativeLocation(FVector(0, 0, 75));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetWorldSize(24);
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Interactable = CreateDefaultSubobject<USPInteractableComponent>(TEXT("Interactable"));
}
void ASPStealthTestConsole::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Label->SetText(ButtonLabel);
}
bool ASPStealthTestConsole::CanUse(APawn* User)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    return IsValid(User) && User->IsPlayerControlled() && TActorIterator<ASPStealthTestDirector>(GetWorld());
#else
    return false;
#endif
}
void ASPStealthTestConsole::BeginPlay()
{
    Super::BeginPlay();
    Interactable->CanInteractNative.BindUObject(this, &ThisClass::CanUse);
    Interactable->OnCompletedNative.AddUObject(this, &ThisClass::Completed);
}
void ASPStealthTestConsole::Completed(APawn* User)
{
    if (!HasAuthority()) { return; }
    for (TActorIterator<ASPStealthTestDirector> It(GetWorld()); It; ++It)
    {
        It->Execute(Command, IncidentKind, User, IncidentLocation, IdentityScope, DispatchGroup, ResponseRadius, MaxResponders);
        break;
    }
}

