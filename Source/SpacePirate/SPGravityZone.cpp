#include "SPGravityZone.h"
#include "SPDebug.h"
#include "SPGravityWorldSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ASPGravityZone::ASPGravityZone()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    // NOTICE [GRAVITY-SCALE]: 적은 수의 영역을 전제로 한다. 대규모 맵에서는 관련성 정책을 재검토한다.
    bAlwaysRelevant = true;
    Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
    SetRootComponent(Bounds);
    Bounds->InitBoxExtent(FVector(500.0f, 500.0f, 300.0f));
    // Overlap 이벤트가 아닌 중심점 판정이므로 충돌은 필요 없다.
    Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bounds->SetGenerateOverlapEvents(false);
    Bounds->SetHiddenInGame(true);
}

void ASPGravityZone::BeginPlay()
{
    // BP BeginPlay에서도 올바른 초기값을 읽도록 먼저 설정한다.
    if (HasAuthority())
    {
        SetGravityMode(InitialGravityMode);
        if (USPGravityWorldSubsystem* Gravity = GetWorld()->GetSubsystem<USPGravityWorldSubsystem>())
        {
            Gravity->RegisterZone(this);
        }
        else
        {
            SP_DEBUG_LOG(Error, TEXT("%s: Gravity subsystem is missing."), *GetName());
        }
    }
    Super::BeginPlay();
}

void ASPGravityZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority() && GetWorld())
    {
        if (USPGravityWorldSubsystem* Gravity = GetWorld()->GetSubsystem<USPGravityWorldSubsystem>())
        {
            Gravity->UnregisterZone(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void ASPGravityZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASPGravityZone, CurrentGravityMode);
}

bool ASPGravityZone::ContainsPoint(const FVector& WorldLocation) const
{
    if (!Bounds || WorldLocation.ContainsNaN())
    {
        return false;
    }
    const FVector Local = Bounds->GetComponentTransform().InverseTransformPosition(WorldLocation);
    const FVector Extent = Bounds->GetUnscaledBoxExtent();
    return FMath::Abs(Local.X) <= Extent.X
        && FMath::Abs(Local.Y) <= Extent.Y
        && FMath::Abs(Local.Z) <= Extent.Z;
}

bool ASPGravityZone::SetGravityMode(ESPGravityMode NewMode)
{
    if (!HasAuthority())
    {
        SP_DEBUG_LOG(Warning, TEXT("%s: Only the server can change zone gravity."), *GetName());
        return false;
    }
    if (NewMode != ESPGravityMode::Gravity && NewMode != ESPGravityMode::ZeroGravity)
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Unsupported gravity mode."), *GetName());
        return false;
    }
    if (CurrentGravityMode == NewMode)
    {
        return true;
    }
    CurrentGravityMode = NewMode;
    FlushNetDormancy();
    ForceNetUpdate();
    // 서버에서는 RepNotify가 자동 호출되지 않는다.
    OnGravityModeChanged.Broadcast(CurrentGravityMode);
    return true;
}

bool ASPGravityZone::TryToggleGravity()
{
    if (!HasAuthority() || !GetWorld())
    {
        return false;
    }
    const double Now = GetWorld()->GetTimeSeconds();
    if (Now - LastSwitchTime < FMath::Max(SwitchCooldown, 0.0f))
    {
        return false; // 정상적인 연타는 로그를 남기지 않는다.
    }
    const ESPGravityMode Next = CurrentGravityMode == ESPGravityMode::Gravity
        ? ESPGravityMode::ZeroGravity : ESPGravityMode::Gravity;
    // 델리게이트가 재진입하더라도 이번 사용을 먼저 기록한다.
    LastSwitchTime = Now;
    return SetGravityMode(Next);
}

void ASPGravityZone::OnRep_GravityMode()
{
    OnGravityModeChanged.Broadcast(CurrentGravityMode);
}
