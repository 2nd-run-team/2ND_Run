// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 로컬·서버 상호작용 시작에 다운 검사를 추가하고 다운 중 진행 작업을 취소한다.

#include "SPInteractorComponent.h"

#include "SPInteractableComponent.h"
#include "SPPlayerStatusComponent.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

USPInteractorComponent::USPInteractorComponent()
{
    // 진행 바 표시는 로컬 플레이어에서만 하지만, 빙의 시점이 늦을 수 있어 틱에서 확인한다.
    PrimaryComponentTick.bCanEverTick = true;
    SetIsReplicatedByDefault(true);
}

void USPInteractorComponent::Press()
{
    if (!CanOwnerInteract())
    {
        return;
    }

    // 클라이언트에서 미리 걸러 불필요한 RPC를 줄인다. 최종 판정은 서버가 같은 규칙으로 다시 한다.
    USPInteractableComponent* Target = FindTargetInView();
    if (Target && !Target->GetCurrentUser() && Target->CanInteract(GetOwner<APawn>()))
    {
        StartInteract(Target);
    }
}

bool USPInteractorComponent::CanOwnerInteract() const
{
    const APawn* OwnerPawn = GetOwner<APawn>();
    const USPPlayerStatusComponent* Life = OwnerPawn ? OwnerPawn->FindComponentByClass<USPPlayerStatusComponent>() : nullptr;
    return OwnerPawn && (!Life || !Life->IsDowned());
}

USPInteractableComponent* USPInteractorComponent::FindTargetInView() const
{
    const APawn* OwnerPawn = GetOwner<APawn>();
    const AController* Controller = OwnerPawn ? OwnerPawn->GetController() : nullptr;
    if (!Controller)
    {
        return nullptr;
    }

    // 몸 기준 눈높이는 무중력에서 몸이 기울면 화면과 어긋나므로, 실제 카메라 시점에서 쏜다.
    FVector ViewLocation;
    FRotator ViewRotation;
    Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);

    const FCollisionQueryParams Params(SCENE_QUERY_STAT(SPInteractTrace), false, OwnerPawn);

    FHitResult Hit;
    const bool bHit = GetWorld()->SweepSingleByChannel(
        Hit,
        ViewLocation,
        ViewLocation + ViewRotation.Vector() * TraceDistance,
        FQuat::Identity,
        TraceChannel,
        FCollisionShape::MakeSphere(TraceRadius),
        Params);

    return bHit && Hit.GetActor()
        ? Hit.GetActor()->FindComponentByClass<USPInteractableComponent>()
        : nullptr;
}

void USPInteractorComponent::StartInteract(USPInteractableComponent* Target)
{
    if (!Target || IsHolding() || !CanOwnerInteract())
    {
        return;
    }

    // 짧게 누르는 대상은 기억할 필요가 없다.
    HoldTarget = Target->IsInstant() ? nullptr : Target;
    ServerStartInteract(Target->GetOwner());
}

void USPInteractorComponent::StopInteract()
{
    // 리슨 서버의 호스트는 서버 처리가 같은 객체에서 바로 실행되므로, 대상을 지우기 전에 보낸다.
    if (HoldTarget)
    {
        ServerStopInteract();
        HoldTarget = nullptr;
    }
}

void USPInteractorComponent::ServerStartInteract_Implementation(AActor* TargetActor)
{
    APawn* OwnerPawn = GetOwner<APawn>();
    USPInteractableComponent* Target =
        TargetActor ? TargetActor->FindComponentByClass<USPInteractableComponent>() : nullptr;

    if (!OwnerPawn || !Target || IsHolding() || !CanOwnerInteract())
    {
        return;
    }

    HoldTarget = Target->TryInteract(OwnerPawn, ServerInteractRange) ? Target : nullptr;
}

void USPInteractorComponent::ServerStopInteract_Implementation()
{
    if (HoldTarget)
    {
        HoldTarget->CancelBy(GetOwner<APawn>());
        HoldTarget = nullptr;
    }
}

bool USPInteractorComponent::IsHolding() const
{
    return HoldTarget && HoldTarget->GetCurrentUser() == GetOwner();
}

bool USPInteractorComponent::IsControlLocked() const
{
    return IsHolding() && HoldTarget->bLockControls;
}

float USPInteractorComponent::GetHoldProgress() const
{
    return IsHolding() ? HoldTarget->GetProgress() : 0.0f;
}

FText USPInteractorComponent::GetHoldPrompt() const
{
    return IsHolding() ? HoldTarget->Prompt : FText::GetEmpty();
}

void USPInteractorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    const APawn* OwnerPawn = GetOwner<APawn>();
    if (OwnerPawn && OwnerPawn->HasAuthority() && !CanOwnerInteract())
    {
        ServerStopInteract_Implementation();
    }
    if (OwnerPawn && OwnerPawn->IsLocallyControlled())
    {
        UpdateHoldDisplay();
    }
}

void USPInteractorComponent::UpdateHoldDisplay()
{
    if (!HoldProgressWidgetClass)
    {
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
        // [TEMP-HOLD-DEBUG] WBP_SPHoldProgress를 지정하면 이 표시는 나오지 않는다.
        // PIE 창들은 GEngine 메시지를 공유해 다른 창에 그려질 수 있으므로 소유자 이름으로 구분한다.
        if (GEngine && IsHolding())
        {
            GEngine->AddOnScreenDebugMessage(
                static_cast<uint64>(GetUniqueID()), 0.0f, FColor::Yellow,
                FString::Printf(TEXT("%s  E 길게: %s  %3.0f%%"),
                    *GetNameSafe(GetOwner()), *GetHoldPrompt().ToString(), GetHoldProgress() * 100.0f));
        }
#endif
        return;
    }

    if (!HoldWidget)
    {
        APlayerController* PlayerController = GetOwner<APawn>()->GetController<APlayerController>();
        if (!PlayerController)
        {
            return;
        }

        HoldWidget = CreateWidget<UUserWidget>(PlayerController, HoldProgressWidgetClass);
        HoldWidget->AddToViewport();
    }

    HoldWidget->SetVisibility(IsHolding()
        ? ESlateVisibility::HitTestInvisible
        : ESlateVisibility::Collapsed);
}

void USPInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 접속 종료 등으로 누르던 사람이 사라지면 대상이 사용 중으로 남지 않게 한다.
    if (HoldTarget && GetOwner() && GetOwner()->HasAuthority())
    {
        HoldTarget->CancelBy(GetOwner<APawn>());
    }

    if (HoldWidget)
    {
        HoldWidget->RemoveFromParent();
        HoldWidget = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}
