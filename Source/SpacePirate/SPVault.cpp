#include "SPVault.h"

#include "SPCargo.h"
#include "SPInteractableComponent.h"
#include "SPInventoryComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

namespace
{
    bool HasDrillBag(const APawn* User)
    {
        const USPInventoryComponent* Inventory = User ? User->FindComponentByClass<USPInventoryComponent>() : nullptr;
        return Inventory && Inventory->FindItemOfType(ESPItemType::DrillBag);
    }
}

ASPVault::ASPVault()
{
    // 드릴이 작동하는 동안만 서버에서 켠다.
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    bIsClueWhenOpen = true;

    // 부모의 Interactable이 직접 해제다. 값은 BP에서 바꿀 수 있다.
    Interactable->HoldDuration = 30.0f;
    Interactable->bKeepProgress = true;
    Interactable->bLockControls = true;
    Interactable->Prompt = NSLOCTEXT("SpacePirate", "VaultDirectPrompt", "직접 해제");
    Interactable->CrimeKind = ESPCrimeKind::VaultWork;

    DrillInstall = CreateDefaultSubobject<USPInteractableComponent>(TEXT("DrillInstall"));
    DrillInstall->HoldDuration = 3.0f;
    DrillInstall->Prompt = NSLOCTEXT("SpacePirate", "VaultDrillPrompt", "드릴 설치");
    DrillInstall->CrimeKind = ESPCrimeKind::VaultWork;

    DrillMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DrillMesh"));
    DrillMesh->SetupAttachment(BodyMesh);
    DrillMesh->SetVisibility(false);
    DrillMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASPVault::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASPVault, DrillEndTime);
}

void ASPVault::BeginPlay()
{
    Super::BeginPlay();

    // 클라이언트도 요청 전에 같은 조건으로 미리 거르고 남은 시간을 안내하므로 양쪽에서 묶는다.
    DrillInstall->CanInteractNative.BindUObject(this, &ASPVault::CanInstallDrill);
    DrillInstall->OnCompletedNative.AddUObject(this, &ASPVault::HandleDrillInstalled);
    Interactable->BlockedPromptNative.BindUObject(this, &ASPVault::GetDirectBlockedPrompt);
}

bool ASPVault::CanOpen(APawn* User) const
{
    // 드릴 가방을 멘 사람은 설치가 골라지도록 직접 해제에서 뺀다. 금고 전체에서 한 명만 조작한다.
    return Super::CanOpen(User) && !IsDrilling() && !DrillInstall->GetCurrentUser() && !HasDrillBag(User);
}

bool ASPVault::CanInstallDrill(APawn* User) const
{
    return !IsOpen() && DrillEndTime <= 0.0 && !Interactable->GetCurrentUser() && HasDrillBag(User);
}

void ASPVault::HandleDrillInstalled(APawn* User)
{
    USPInventoryComponent* Inventory = User ? User->FindComponentByClass<USPInventoryComponent>() : nullptr;
    ASPCargo* Bag = Inventory ? Inventory->FindItemOfType(ESPItemType::DrillBag) : nullptr;
    if (!Bag)
    {
        return;
    }

    Inventory->ConsumeItem(Bag);

    // 직접 해제로 쌓인 비율만큼 덜 작업한다.
    DrillRemainingSeconds = DrillSeconds * (1.0f - Interactable->GetProgress());
    DrillEndTime = GetSyncedTime() + DrillRemainingSeconds;
    OnRep_DrillEndTime();
    SetActorTickEnabled(true);
}

void ASPVault::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority() || !IsDrilling())
    {
        SetActorTickEnabled(false);
        return;
    }

    DrillRemainingSeconds -= DeltaSeconds;
    if (DrillRemainingSeconds <= 0.0f)
    {
        Open();
        SetActorTickEnabled(false);
    }
}

FText ASPVault::GetDirectBlockedPrompt(APawn* User) const
{
    if (!IsDrilling())
    {
        return Interactable->BlockedPrompt;
    }

    const int32 Seconds = FMath::Max(0, FMath::CeilToInt(static_cast<float>(DrillEndTime - GetSyncedTime())));
    return FText::Format(NSLOCTEXT("SpacePirate", "VaultDrilling", "드릴 작동 중 · 남은 {0}초"), Seconds);
}

void ASPVault::OnRep_DrillEndTime()
{
    DrillMesh->SetVisibility(DrillEndTime > 0.0);
}

double ASPVault::GetSyncedTime() const
{
    const UWorld* World = GetWorld();
    const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
    return GameState ? GameState->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0);
}
