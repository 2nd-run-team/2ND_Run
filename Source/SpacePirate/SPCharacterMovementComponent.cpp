// 작업자: 김세훈 | 2026-10-08 | 플레이어 상태 MVP 수정
// 변경 내용: 다운 상태에서 이동 틱·서버 이동 패킷·중력 전환을 제한하고 잔여 이동 입력을 정리한다.
// 작업자: 김세훈 | 2026-10-08 | 다운 캡슐 정렬 수정
// 변경 내용: 캡슐을 충돌 검사 후 옆으로 눕히고 지상 높이를 조정하며 소유 클라이언트에도 회전을 보정한다.

#include "SPCharacterMovementComponent.h"
#include "SPDebug.h"
#include "SPGravityWorldSubsystem.h"
#include "SPInventoryComponent.h"
#include "SPPlayerCharacter.h"
#include "Engine/ScopedMovementUpdate.h"

#include "Components/CapsuleComponent.h"
#include "Engine/NetSerialization.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementReplication.h"

// NOTICE [ZG-PROTOTYPE] 정식 이동 확장 시 재검토할 항목:
// 1. FSavedMove_SP::CanCombineWith/SetMoveFor: 무중력 기록 병합을 막아 회전/추진의 시간 순서를 보존한다.
//    대역폭 최적화 시 두 곳을 함께 검토하고 지연/패킷 손실 환경에서 예측 오차를 확인한다.
// 2. TrySetBodyRotation: 중간 자세의 겹침 검사이며 연속 회전 Sweep이 아니다. 얇은 지형은 별도 검증한다.
// 3. GetSimulationViewRotation: Roll=0인 시선 추종 방식. 자유 회전 추가 시 입력/카메라와 함께 확장한다.
// 4. FSPNetworkMoveData::Serialize: 0.1 단위 입력 압축은 현재 디지털 키보드 입력을 전제로 한다.
// 위 항목은 현재 이동의 일부이므로 Shipping/Test에서도 필요하다. 임시 테스트 키와는 별개다.

// ------------------------------------------------------------
// 클라이언트 이동 기록
// ------------------------------------------------------------

// 소유 클라이언트는 먼저 이동하고 입력을 보관한다. 서버 보정 후에는 같은 입력을 재생한다.
// 커스텀 값도 함께 보관하지 않으면 과거 이동을 현재 키 상태로 재계산하게 된다.
class FSavedMove_SP final : public FSavedMove_Character
{
public:
    using Super = FSavedMove_Character;

    bool bSavedSprintRequested = false;
    bool bSavedCustomGravityMovement = false;

    FVector SavedLocalThrustInput = FVector::ZeroVector;

    virtual void Clear() override
    {
        // SavedMove는 재사용되므로 이전 이동의 커스텀 입력까지 초기화한다.
        Super::Clear();

        bSavedSprintRequested = false;
        bSavedCustomGravityMovement = false;
        SavedLocalThrustInput = FVector::ZeroVector;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Flags = Super::GetCompressedFlags();

        if (bSavedSprintRequested)
        {
            // 엔진 점프/앉기 플래그와 겹치지 않는 사용자 비트로 달리기 요청을 전달한다.
            Flags |= FLAG_Custom_0;
        }

        return Flags;
    }

    virtual bool CanCombineWith(
        const FSavedMovePtr& NewMove,
        ACharacter* Character,
        float MaxDelta) const override
    {
        const FSavedMove_SP* NewSPMove =
            static_cast<const FSavedMove_SP*>(NewMove.Get());

        // 무중력/복귀 상태의 이동 기록은 합치지 않는다.
        // 몸 회전과 추진 방향을 각 이동 시점에 맞춰 계산한다.
        if (bSavedCustomGravityMovement || NewSPMove->bSavedCustomGravityMovement)
        {
            return false;
        }

        if (bSavedSprintRequested != NewSPMove->bSavedSprintRequested)
        {
            return false;
        }

        if (!SavedLocalThrustInput.Equals(
            NewSPMove->SavedLocalThrustInput, 0.001f))
        {
            return false;
        }

        if (!StartControlRotation.Equals(
            NewSPMove->StartControlRotation, 0.01f))
        {
            return false;
        }

        return Super::CanCombineWith(
            NewMove, Character, MaxDelta);
    }

    virtual bool IsImportantMove(
        const FSavedMovePtr& LastAckedMove) const override
    {
        const FSavedMove_SP* LastSPMove =
            static_cast<const FSavedMove_SP*>(LastAckedMove.Get());

        // 입력 시작/해제를 중요한 이동으로 표시해 손실된 기록의 재전송 후보에 포함한다.
        if (bSavedCustomGravityMovement != LastSPMove->bSavedCustomGravityMovement
            || !SavedLocalThrustInput.Equals(
                LastSPMove->SavedLocalThrustInput, 0.001f))
        {
            return true;
        }

        return Super::IsImportantMove(LastAckedMove);
    }

    virtual void SetMoveFor(
        ACharacter* Character,
        float InDeltaTime,
        const FVector& NewAcceleration,
        FNetworkPredictionData_Client_Character& ClientData) override
    {
        Super::SetMoveFor(
            Character,
            InDeltaTime,
            NewAcceleration,
            ClientData);

        const USPCharacterMovementComponent* Movement =
            CastChecked<USPCharacterMovementComponent>(
                Character->GetCharacterMovement());

        // 이 이동을 시작할 때의 입력을 캡처한다. 완료 후 몸 회전은 부모가 SavedRotation에 저장한다.
        bSavedSprintRequested = Movement->IsSprintRequested();
        bSavedCustomGravityMovement = Movement->IsCustomGravityMovement();
        SavedLocalThrustInput = Movement->GetZeroGravityInput();

        bForceNoCombine |= bSavedCustomGravityMovement;
    }

    virtual void PrepMoveFor(ACharacter* Character) override
    {
        Super::PrepMoveFor(Character);

        USPCharacterMovementComponent* Movement =
            CastChecked<USPCharacterMovementComponent>(
                Character->GetCharacterMovement());

        Movement->SetSprintRequested(bSavedSprintRequested);
        Movement->SetZeroGravityInput(SavedLocalThrustInput);

        // 몸 회전을 과거 클라이언트 값으로 덮어쓰지 않는다.
        // 서버가 보정한 회전에서 다음 이동을 재계산해야 한다.
    }
};

// 저장된 예측 기록을 송신 데이터로 복사한다. 현재 프레임의 입력을 직접 읽지 않는다.
void FSPNetworkMoveData::ClientFillNetworkMoveData(
    const FSavedMove_Character& ClientMove,
    ENetworkMoveType MoveType)
{
    FCharacterNetworkMoveData::ClientFillNetworkMoveData(
        ClientMove, MoveType);

    const FSavedMove_SP& SPMove =
        static_cast<const FSavedMove_SP&>(ClientMove);

    LocalThrustInput = SPMove.SavedLocalThrustInput;

    // 부모 SavedMove가 이동 완료 후 저장한 실제 몸 회전.
    EndBodyRotation = SPMove.SavedRotation;
}

bool FSPNetworkMoveData::Serialize(
    UCharacterMovementComponent& CharacterMovement,
    FArchive& Ar,
    UPackageMap* PackageMap,
    ENetworkMoveType MoveType)
{
    const bool bBaseSuccess =
        FCharacterNetworkMoveData::Serialize(
            CharacterMovement, Ar, PackageMap, MoveType);

    // 송신/수신이 동일한 순서로 읽고 쓴다. 디지털 입력은 정확히 보존하고 회전은 압축한다.
    FVector_NetQuantize10 PackedInput(LocalThrustInput);

    bool bInputSuccess = true;
    PackedInput.NetSerialize(Ar, PackageMap, bInputSuccess);

    EndBodyRotation.SerializeCompressedShort(Ar);

    if (Ar.IsLoading())
    {
        LocalThrustInput = PackedInput;

        if (LocalThrustInput.ContainsNaN()
            || EndBodyRotation.ContainsNaN())
        {
            Ar.SetError();
        }
    }

    return bBaseSuccess && bInputSuccess && !Ar.IsError();
}

// ------------------------------------------------------------
// 커스텀 이동 기록 생성
// ------------------------------------------------------------

class FNetworkPredictionData_Client_SP final
    : public FNetworkPredictionData_Client_Character
{
public:
    explicit FNetworkPredictionData_Client_SP(
        const UCharacterMovementComponent& ClientMovement)
        : FNetworkPredictionData_Client_Character(ClientMovement)
    {}

    virtual FSavedMovePtr AllocateNewMove() override
    {
        // 아래 static_cast들이 안전하려면 모든 이동 기록이 이 커스텀 타입이어야 한다.
        return FSavedMovePtr(new FSavedMove_SP());
    }
};

// ------------------------------------------------------------
// 이동 컴포넌트
// ------------------------------------------------------------

USPCharacterMovementComponent::USPCharacterMovementComponent()
{
    // RPC를 매 프레임 별도로 보내지 않고 엔진 이동 패킷에 추진 입력/몸 회전을 함께 싣는다.
    SetNetworkMoveDataContainer(SPNetworkMoveDataContainer);

    // 엔진이 이미 UPROPERTY로 노출한 값의 초기값이다. BP의 Character Movement에서 덮어쓸 수 있다.
    MaxWalkSpeed = 400.0f;
    SprintSpeed = 700.0f;
    GetNavAgentPropertiesRef().bCanCrouch = true;
    SetCrouchedHalfHeight(56.0f);
    MaxWalkSpeedCrouched = 160.0f;

    MaxAcceleration = 2048.0f;
    BrakingDecelerationWalking = 2048.0f;
    GroundFriction = 8.0f;

    JumpZVelocity = 420.0f;
    GravityScale = 1.0f;

    // 이번 버전은 공중 방향 조작을 사용하지 않는다.
    AirControl = 0.0f;
    FallingLateralFriction = 0.0f;
    BrakingDecelerationFalling = 0.0f;

    // 이동 방향으로 자동 회전하면 관성 이동 중 몸과 시선을 따로 돌릴 수 없으므로 끈다.
    bOrientRotationToMovement = false;
    bUseControllerDesiredRotation = false;
}

// 작업자: 김세훈 | 정지한 서버 캐릭터도 버튼 전환과 영역 제거를 반영한다.
void USPCharacterMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    if (const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(CharacterOwner); Player && Player->IsDowned())
    {
        StopMovementImmediately();
        DisableMovement();
    }
    RefreshGravityFromZones();
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// 작업자: 김세훈 | 원격 입력 처리에서도 현재 서버 위치의 환경을 확인한다.
void USPCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
    if (const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(CharacterOwner); Player && Player->IsDowned())
    {
        bWantsToCrouch = CharacterOwner->bIsCrouched;
        CharacterOwner->StopJumping();
        bSprintRequested = false;
        LocalThrustInput = FVector::ZeroVector;
        StopMovementImmediately();
        DisableMovement();
        return;
    }
    Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
    RefreshGravityFromZones();
}

// 작업자: 김세훈 | 조회는 서버 내부 처리이며 상태가 다를 때만 네트워크 전환을 요청한다.
void USPCharacterMovementComponent::RefreshGravityFromZones()
{
    if (!HasValidData() || !CharacterOwner->HasAuthority()
        || !CharacterOwner->HasActorBegunPlay() || MovementMode == MOVE_None)
    {
        return;
    }
    const USPGravityWorldSubsystem* Gravity = GetWorld()->GetSubsystem<USPGravityWorldSubsystem>();
    if (!Gravity)
    {
        return;
    }
    // NOTICE [GRAVITY-SCALE]: 플레이어/소수 영역을 위한 전체 목록 조회.
    // 대량 화물 연결 전 후보 영역 캐시를 검토한다. 원격 이동 한 Tick에 여러 조회가 가능하다.
    const ESPGravityMode Desired = Gravity->GetGravityModeAtLocation(UpdatedComponent->GetComponentLocation());
    if (Desired != GetGravityMode())
    {
        SetGravityMode(Desired);
    }
}

void USPCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!CharacterOwner)
    {
        SP_DEBUG_LOG(Error, TEXT("%s: Movement initialized without a Character owner. Attach this component through SPPlayerCharacter's default movement component."), *GetName());
        return;
    }

    if (SprintSpeed < MaxWalkSpeed)
    {
        SP_DEBUG_LOG(Warning, TEXT("%s: SprintSpeed (%.1f) is below MaxWalkSpeed (%.1f); GetMaxSpeed will use MaxWalkSpeed for sprinting. Check Blueprint movement defaults."),
            *GetNameSafe(CharacterOwner), SprintSpeed, MaxWalkSpeed);
    }
}

void USPCharacterMovementComponent::SetSprintRequested(
    bool bRequested)
{
    bSprintRequested = bRequested;
}

bool USPCharacterMovementComponent::HasForwardAcceleration() const
{
    if (!CharacterOwner)
    {
        return false;
    }

    const FVector InputDirection = Acceleration.GetSafeNormal2D();

    if (InputDirection.IsNearlyZero())
    {
        return false;
    }

    FRotator ViewRotation = CharacterOwner->GetControlRotation();

    // 서버 이동 처리와 클라이언트 재시뮬레이션에서는
    // 해당 이동 기록의 시선 방향을 사용한다.
    if (const FCharacterNetworkMoveData* MoveData =
        GetCurrentNetworkMoveData())
    {
        ViewRotation = MoveData->ControlRotation;
    }

    const FVector Forward =
        FRotator(0.0f, ViewRotation.Yaw, 0.0f).Vector();

    const float ForwardDot =
        FVector::DotProduct(InputDirection, Forward);

    // 전진: 1.0
    // 전진 대각선: 약 0.707
    // 옆: 0.0
    // 후진: -1.0
    return ForwardDot > FMath::Clamp(SprintForwardDotThreshold, 0.0f, 1.0f);
}

float USPCharacterMovementComponent::GetMaxSpeed() const
{
    if (const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(CharacterOwner); Player && Player->IsDowned())
    {
        return 0.0f;
    }
    if (IsGravityRecovery())
    {
        return FMath::Max(MaxWalkSpeed, 0.0f);
    }

    if (IsZeroGravity())
    {
        return FMath::Max(ZeroGravityMaxSpeed, 1.0f);
    }

    const float CarryMultiplier =
        IsWearingBag() ? FMath::Clamp(BagSpeedMultiplier, 0.1f, 1.0f) : 1.0f;

    return GetGravityMaxSpeed() * CarryMultiplier;
}

bool USPCharacterMovementComponent::IsWearingBag() const
{
    // ponytail: 인벤토리는 서버에서 복제되므로 가방을 메거나 내려놓은 직후 한 번 짧은 위치 보정이 생길 수 있다.
    // 거슬리면 가방 여부를 이동 기록(SavedMove/NetworkMoveData)에 넣어 예측에 포함시킨다.
    const ASPPlayerCharacter* SPOwner =
        Cast<ASPPlayerCharacter>(CharacterOwner);

    const USPInventoryComponent* Inventory =
        SPOwner ? SPOwner->GetInventory() : nullptr;

    return Inventory && Inventory->HasBag();
}

float USPCharacterMovementComponent::GetGravityMaxSpeed() const
{
    // 가방을 멘 동안에는 달리기를 눌러도 걷기 속도다.
    const float EffectiveSprintSpeed = IsWearingBag()
        ? MaxWalkSpeed
        : FMath::Max(MaxWalkSpeed, SprintSpeed);

    if (IsMovingOnGround())
    {
        if (CharacterOwner && CharacterOwner->bIsCrouched)
        {
            return Super::GetMaxSpeed();
        }

        if (bSprintRequested && HasForwardAcceleration())
        {
            return EffectiveSprintSpeed;
        }

        return MaxWalkSpeed;
    }

    if (IsFalling())
    {
        // 달리기 점프의 수평 속도를 허용하는 상한.
        // 실제 속도를 이 값으로 변경하는 코드는 아니다.
        return EffectiveSprintSpeed;
    }

    return Super::GetMaxSpeed();
}

void USPCharacterMovementComponent::UpdateFromCompressedFlags(
    uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    bSprintRequested =
        (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;

    if (IsCustomGravityMovement() && CharacterOwner)
    {
        // 무중력의 Space는 점프가 아니라 추진 입력이다.
        CharacterOwner->StopJumping();
    }
}

FNetworkPredictionData_Client*
USPCharacterMovementComponent::GetPredictionData_Client() const
{
    // 첫 예측 요청 때 한 번 생성한다. 엔진이 ClientPredictionData의 수명을 관리한다.
    if (!ClientPredictionData)
    {
        USPCharacterMovementComponent* MutableThis =
            const_cast<USPCharacterMovementComponent*>(this);

        MutableThis->ClientPredictionData =
            new FNetworkPredictionData_Client_SP(*this);

    }

    return ClientPredictionData;
}

bool USPCharacterMovementComponent::
ClientUpdatePositionAfterServerUpdate()
{
    // Super 안에서 과거 입력을 여러 번 복원한다. 재생 후에는 현재 누른 입력으로 되돌린다.
    const bool bCurrentSprintRequest = bSprintRequested;
    const FVector CurrentThrustInput = LocalThrustInput;

    const bool bUpdated =
        Super::ClientUpdatePositionAfterServerUpdate();

    bSprintRequested = bCurrentSprintRequest;
    LocalThrustInput = CurrentThrustInput;

    return bUpdated;
}

void USPCharacterMovementComponent::SetZeroGravityInput(
    const FVector& Input)
{
    if (Input.ContainsNaN())
    {
        SP_DEBUG_LOG(
            Error,
            TEXT("%s: Invalid zero-gravity input. Input was cleared."),
            *GetNameSafe(CharacterOwner));

        LocalThrustInput = FVector::ZeroVector;
        return;
    }

    // 현재 디지털 키보드 입력에 맞춰 각 축을 -1~1로 제한.
    LocalThrustInput = FVector(
        FMath::Clamp(Input.X, -1.0, 1.0),
        FMath::Clamp(Input.Y, -1.0, 1.0),
        FMath::Clamp(Input.Z, -1.0, 1.0));
}

bool USPCharacterMovementComponent::SetGravityMode(
    ESPGravityMode NewMode)
{
    if (const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(CharacterOwner); Player && Player->IsDowned())
    {
        return false;
    }
    if (!HasValidData())
    {
        SP_DEBUG_LOG(
            Error,
            TEXT("%s: Gravity change rejected: invalid movement data."),
            *GetName());

        return false;
    }

    if (!CharacterOwner->HasAuthority())
    {
        SP_DEBUG_LOG(
            Warning,
            TEXT("%s: Gravity change rejected: only the server can change gravity mode."),
            *GetNameSafe(CharacterOwner));

        return false;
    }

    if (NewMode != ESPGravityMode::Gravity
        && NewMode != ESPGravityMode::ZeroGravity)
    {
        SP_DEBUG_LOG(
            Warning,
            TEXT("%s: Gravity change rejected: unsupported mode."),
            *GetNameSafe(CharacterOwner));

        return false;
    }

    if (GetGravityMode() == NewMode)
    {
        return true;
    }

    // 모드 전환으로 엔진 내부 값이 바뀌어도 월드 속도는 유지한다. 방향을 몸 쪽으로 돌리지 않는다.
    const FVector PreviousVelocity = Velocity;

    if (NewMode == ESPGravityMode::ZeroGravity)
    {
        SetMovementMode(MOVE_Custom, ZeroGravityCustomMode);
    }
    else
    {
        // 환경 중력은 즉시 적용한다. 직립 불가 시에는 복귀 상태에서 낙하/수평 이동한다.
        SetMovementMode(MOVE_Custom, GravityRecoveryCustomMode);
    }

    Velocity = PreviousVelocity;

    CharacterOwner->StopJumping();
    bSprintRequested = false;

    CharacterOwner->ForceNetUpdate();

    // 소유 클라이언트에도 새 모드·위치·회전을 보정으로 전달.
    ForceClientAdjustment();

    return true;
}

bool USPCharacterMovementComponent::TryEnterDownedPose()
{
    if (!HasValidData() || !CharacterOwner->HasAuthority())
    {
        return false;
    }

    const bool bWasOnGround = IsMovingOnGround();
    const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    const FQuat PreviousRotation = UpdatedComponent->GetComponentQuat();
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float SegmentHalfLength = Capsule->GetScaledCapsuleHalfHeight() - Radius;
    const float PreviousVerticalExtent = Radius + SegmentHalfLength * FMath::Abs(PreviousRotation.GetUpVector().Z);

    // 메시만 회전시키지 않고 루트 캡슐을 회전시켜 몸과 상호작용 판정이 함께 눕게 한다.
    // 회전 경로에 벽/동료가 있으면 반대쪽을 시도하며, 두 방향 모두 막히면 관통시키지 않는다.
    // 이전 복구 틱 전에 다시 다운돼도 회전이 누적되지 않도록 Yaw 기준의 절대 자세를 만든다.
    const FQuat UprightRotation = FRotator(0.0, PreviousRotation.Rotator().Yaw, 0.0).Quaternion();
    const FQuat SideRotation = UprightRotation * FQuat(FVector::ForwardVector, HALF_PI);
    const FQuat OppositeRotation = UprightRotation * FQuat(FVector::ForwardVector, -HALF_PI);
    if (!TrySetBodyRotation(SideRotation) && !TrySetBodyRotation(OppositeRotation))
    {
        return false;
    }

    if (bWasOnGround)
    {
        const float DownVerticalExtent = Radius + SegmentHalfLength * FMath::Abs(UpdatedComponent->GetUpVector().Z);
        const float LowerDistance = FMath::Max(0.0f, PreviousVerticalExtent - DownVerticalExtent);
        FHitResult Hit;
        SafeMoveUpdatedComponent(FVector(0.0, 0.0, -LowerDistance), UpdatedComponent->GetComponentQuat(), true, Hit);
    }

    bJustTeleported = true;
    CharacterOwner->ForceNetUpdate();
    // AutonomousProxy는 일반 액터 이동 복제를 받지 않으므로 기존 위치/회전 보정도 요청한다.
    ForceClientAdjustment();
    return true;
}

FRotator USPCharacterMovementComponent::
GetSimulationViewRotation() const
{
    FRotator Result = CharacterOwner
        ? CharacterOwner->GetControlRotation()
        : FRotator::ZeroRotator;

    if (const FCharacterNetworkMoveData* MoveData =
        GetCurrentNetworkMoveData())
    {
        Result = MoveData->ControlRotation;
    }

    Result.Normalize();
    // 카메라와 동일한 BP 설정을 사용한다. 서버도 패킷의 시선 범위를 다시 제한한다.
    Result.Pitch = FMath::Clamp(Result.Pitch,
        static_cast<double>(FMath::Clamp(ZeroGravityPitchMin, -89.0f, 0.0f)),
        static_cast<double>(FMath::Clamp(ZeroGravityPitchMax, 0.0f, 89.0f)));
    Result.Roll = 0.0;

    return Result;
}

bool USPCharacterMovementComponent::CanOccupyRotation(
    const FQuat& Rotation) const
{
    if (!HasValidData())
    {
        return false;
    }

    const UCapsuleComponent* Capsule =
        CharacterOwner->GetCapsuleComponent();

    return !OverlapTest(
        UpdatedComponent->GetComponentLocation(),
        Rotation,
        Capsule->GetCollisionObjectType(),
        Capsule->GetCollisionShape(),
        CharacterOwner);
}

bool USPCharacterMovementComponent::TrySetBodyRotation(
    const FQuat& TargetRotation)
{
    if (!HasValidData())
    {
        return false;
    }

    const FQuat StartRotation =
        UpdatedComponent->GetComponentQuat();

    const float AngleDegrees = FMath::RadiansToDegrees(
        StartRotation.AngularDistance(TargetRotation));

    // 회전 이동은 일반적인 직선 Sweep만으로 검사되지 않는다.
    // 중간 자세도 작은 각도 간격으로 확인한다.
    const int32 SampleCount = FMath::Max(
        1,
        FMath::CeilToInt(AngleDegrees / FMath::Clamp(RotationCollisionStepDegrees, 0.1f, 10.0f)));

    for (int32 Index = 1; Index <= SampleCount; ++Index)
    {
        const float Alpha =
            static_cast<float>(Index) / SampleCount;

        const FQuat SampleRotation =
            FQuat::Slerp(
                StartRotation,
                TargetRotation,
                Alpha).GetNormalized();

        if (!CanOccupyRotation(SampleRotation))
        {
            return false;
        }
    }

    MoveUpdatedComponent(
        FVector::ZeroVector,
        TargetRotation,
        false);

    return true;
}

// 작업자: 김세훈 | 직립 실패 시 시험 이동을 취소해 천장/바닥 관통을 방지한다.
bool USPCharacterMovementComponent::TryRestoreUpright()
{
    if (!HasValidData())
    {
        return false;
    }
    const FQuat Upright = FRotator(0.0f, UpdatedComponent->GetComponentRotation().Yaw, 0.0f).Quaternion();
    if (TrySetBodyRotation(Upright))
    {
        return true;
    }
    const UCapsuleComponent* Capsule = CharacterOwner->GetCapsuleComponent();
    if (!Capsule)
    {
        return false;
    }
    const float Radius = Capsule->GetScaledCapsuleRadius();
    const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    const FVector CapsuleUp = UpdatedComponent->GetComponentQuat().GetUpVector();
    const float VerticalHalfHeight = Radius + (HalfHeight - Radius) * FMath::Abs(CapsuleUp.Z);
    // 누운 캡슐을 같은 중심에서 세우면 바닥에 박힐 수 있어 필요한 높이만큼만 상승을 시도한다.
    const float Lift = FMath::Max(0.0f, HalfHeight - VerticalHalfHeight)
        + FMath::Clamp(GravityRecoveryClearance, 0.0f, 5.0f);
    if (Lift <= KINDA_SMALL_NUMBER)
    {
        return false;
    }
    FScopedMovementUpdate ScopedMove(UpdatedComponent, EScopedUpdate::DeferredUpdates);
    FHitResult Hit;
    SafeMoveUpdatedComponent(FVector(0.0f, 0.0f, Lift), UpdatedComponent->GetComponentQuat(), true, Hit);
    if (!Hit.bBlockingHit && !Hit.bStartPenetrating && TrySetBodyRotation(Upright))
    {
        return true;
    }
    ScopedMove.RevertMove();
    return false;
}

// 작업자: 김세훈 | 중력을 적용하면서 직립 가능 공간을 확보한다. 막힌 회전은 정상 충돌이라 기록하지 않는다.
void USPCharacterMovementComponent::PhysGravityRecovery(float DeltaTime, int32 Iterations)
{
    float RemainingTime = DeltaTime;
    const int32 MaxSteps = FMath::Clamp(ZeroGravityMaxSimulationIterations, 1, 128);
    const float MaxStepTime = FMath::Clamp(ZeroGravityMaxSimulationTimeStep, 0.001f, 0.05f);
    for (int32 Step = 0; Step < MaxSteps && RemainingTime > MIN_TICK_TIME; ++Step)
    {
        if (TryRestoreUpright())
        {
            const FVector PreviousVelocity = Velocity;
            SetMovementMode(MOVE_Falling);
            Velocity = PreviousVelocity;
            // 남은 시간은 일반 낙하/착지 처리에 넘긴다.
            StartNewPhysics(RemainingTime, Iterations);
            return;
        }
        const float StepTime = Step == MaxSteps - 1 ? RemainingTime : FMath::Min(RemainingTime, MaxStepTime);
        RemainingTime -= StepTime;
        // 복귀 중에는 시선 Yaw 기준 수평 탈출 입력만 허용한다. Space/Shift 추진은 무시한다.
        const FVector Input = FVector(LocalThrustInput.X, LocalThrustInput.Y, 0.0).GetClampedToMaxSize(1.0f);
        const FQuat Yaw = FRotator(0.0f, GetSimulationViewRotation().Yaw, 0.0f).Quaternion();
        FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0);
        const float PreviousSpeed = HorizontalVelocity.Size();
        HorizontalVelocity += Yaw.RotateVector(Input) * FMath::Max(GravityRecoveryAcceleration, 0.0f) * StepTime;
        HorizontalVelocity = HorizontalVelocity.GetClampedToMaxSize(FMath::Max(FMath::Max(MaxWalkSpeed, 0.0f), PreviousSpeed));
        Velocity.X = HorizontalVelocity.X;
        Velocity.Y = HorizontalVelocity.Y;
        // 종단 속도까지 포함하는 엔진의 낙하 속도 계산을 사용한다.
        Velocity = NewFallVelocity(Velocity, FVector(0.0f, 0.0f, GetGravityZ()), StepTime);
        MoveWithCollision(StepTime);
        if (!HasValidData() || !IsGravityRecovery())
        {
            return;
        }
    }
}

void USPCharacterMovementComponent::RotateZeroGravityBody(
    float DeltaTime)
{
    const FQuat CurrentRotation =
        UpdatedComponent->GetComponentQuat();

    const FQuat TargetRotation =
        GetSimulationViewRotation().Quaternion();

    const float AngleRadians =
        CurrentRotation.AngularDistance(TargetRotation);

    if (AngleRadians <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    // Slerp의 진행 비율을 초당 각도 제한으로 환산해 프레임 시간에 맞춰 천천히 따라간다.
    const float MaxStepRadians =
        FMath::DegreesToRadians(
            FMath::Max(ZeroGravityTurnRate, 0.0f)) * DeltaTime;

    const float Alpha = FMath::Clamp(
        MaxStepRadians / AngleRadians,
        0.0f,
        1.0f);

    const FQuat NewRotation =
        FQuat::Slerp(
            CurrentRotation,
            TargetRotation,
            Alpha).GetNormalized();

    // 주변 지형이 막고 있다면 이번 회전은 진행하지 않는다.
    // 정상적인 충돌 상황이므로 로그를 출력하지 않는다.
    TrySetBodyRotation(NewRotation);
}

void USPCharacterMovementComponent::MoveZeroGravity(
    float DeltaTime)
{
    // 여러 방향 키를 동시에 눌러도 추진력 합계가 커지지 않도록 크기를 1로 제한한다.
    const FVector Input =
        LocalThrustInput.GetClampedToMaxSize(1.0f);

    const FQuat BodyRotation =
        UpdatedComponent->GetComponentQuat();

    // 로컬 X=앞, Y=오른쪽, Z=위.
    const FVector WorldThrust =
        BodyRotation.RotateVector(Input);

    const float PreviousSpeed = Velocity.Size();

    // 핵심: 몸 회전은 새 추진력에만 적용한다. 기존 Velocity는 월드 좌표 그대로 보존한다.
    // 따라서 입력을 놓고 시선/몸만 돌리면 이동 방향은 유지된다(충돌 또는 감쇠가 없는 경우).
    Velocity += WorldThrust
        * FMath::Max(ZeroGravityAcceleration, 0.0f)
        * DeltaTime;

    const float Drag = FMath::Max(ZeroGravityDrag, 0.0f);

    if (Drag > 0.0f)
    {
        Velocity *= FMath::Exp(-Drag * DeltaTime);
    }

    // 중력 전환 전에 이미 빠르게 움직이고 있었다면
    // 모드 변경만으로 그 속도를 갑자기 잘라내지 않는다.
    const float AllowedSpeed = FMath::Max(
        FMath::Max(ZeroGravityMaxSpeed, 1.0f),
        PreviousSpeed);

    Velocity = Velocity.GetClampedToMaxSize(AllowedSpeed);

    MoveWithCollision(DeltaTime);
}

// 작업자: 김세훈 | 무중력/복귀 상태가 동일한 충돌 및 벽 접선 이동을 사용한다.
void USPCharacterMovementComponent::MoveWithCollision(float DeltaTime)
{
    float RemainingMoveTime = DeltaTime;

    // 모서리에서 여러 면과 충돌할 수 있으므로 제한적으로 반복.
    for (int32 ContactIndex = 0;
        ContactIndex < FMath::Clamp(ZeroGravityMaxCollisionIterations, 1, 16)
            && RemainingMoveTime > MIN_TICK_TIME;
        ++ContactIndex)
    {
        FHitResult Hit;

        SafeMoveUpdatedComponent(
            Velocity * RemainingMoveTime,
            UpdatedComponent->GetComponentQuat(),
            true,
            Hit);

        if (!Hit.IsValidBlockingHit())
        {
            if (Hit.bStartPenetrating)
            {
                Velocity = FVector::ZeroVector;
            }

            break;
        }

        // 충돌 이벤트에서 소유자가 사라지거나 모드가 바뀔 수 있어 호출 뒤 상태를 다시 확인한다.
        HandleImpact(
            Hit,
            RemainingMoveTime,
            Velocity * RemainingMoveTime);

        if (!HasValidData() || !IsCustomGravityMovement())
        {
            return;
        }

        RemainingMoveTime *= 1.0f - Hit.Time;

        // 벽 안쪽으로 향하는 속도 성분만 제거.
        const double IntoSurface =
            FVector::DotProduct(Velocity, Hit.Normal);

        if (IntoSurface < 0.0)
        {
            Velocity -= Hit.Normal * IntoSurface;
        }
        else
        {
            break;
        }
    }
}

void USPCharacterMovementComponent::PhysCustom(
    float DeltaTime,
    int32 Iterations)
{
    if (!IsCustomGravityMovement())
    {
        Super::PhysCustom(DeltaTime, Iterations);
        return;
    }

    if (!HasValidData() || DeltaTime < MIN_TICK_TIME)
    {
        return;
    }

    if (Velocity.ContainsNaN())
    {
        SP_DEBUG_LOG(
            Error,
            TEXT("%s: Invalid custom gravity velocity. Movement was stopped."),
            *GetNameSafe(CharacterOwner));

        StopMovementImmediately();
        return;
    }

    if (IsGravityRecovery())
    {
        PhysGravityRecovery(DeltaTime, Iterations);
        return;
    }

    // 몸 회전과 추진을 작은 시간 단위로 함께 계산한다. 서버 처리/클라이언트 예측이 같은 경로를 쓴다.
    // 반복 상한에 도달하면 남은 시간을 마지막에 처리하므로 심한 끊김에서는 정밀도가 낮아질 수 있다.
    float RemainingTime = DeltaTime;
    const int32 MaxSteps = FMath::Clamp(ZeroGravityMaxSimulationIterations, 1, 128);
    const float MaxStepTime = FMath::Clamp(ZeroGravityMaxSimulationTimeStep, 0.001f, 0.05f);

    for (int32 Step = 0;
        Step < MaxSteps && RemainingTime > MIN_TICK_TIME;
        ++Step)
    {
        const float StepTime = Step == MaxSteps - 1
            ? RemainingTime
            : FMath::Min(RemainingTime, MaxStepTime);

        RemainingTime -= StepTime;

        RotateZeroGravityBody(StepTime);
        MoveZeroGravity(StepTime);

        if (!HasValidData() || !IsZeroGravity())
        {
            return;
        }
    }
}

void USPCharacterMovementComponent::MoveAutonomous(
    float ClientTimeStamp,
    float DeltaTime,
    uint8 CompressedFlags,
    const FVector& NewAccel)
{
    if (const ASPPlayerCharacter* Player = Cast<ASPPlayerCharacter>(CharacterOwner); Player && Player->IsDowned())
    {
        LocalThrustInput = FVector::ZeroVector;
        bSprintRequested = false;
        StopMovementImmediately();
        DisableMovement();
        Super::MoveAutonomous(ClientTimeStamp, DeltaTime, 0, FVector::ZeroVector);
        return;
    }
    // 서버는 로컬 키 이벤트를 받지 않는다. 해당 이동 패킷의 몸 기준 입력을 먼저 복원한다.
    if (const FCharacterNetworkMoveData* BaseMoveData =
        GetCurrentNetworkMoveData())
    {
        const FSPNetworkMoveData* SPMoveData =
            static_cast<const FSPNetworkMoveData*>(BaseMoveData);

        SetZeroGravityInput(SPMoveData->LocalThrustInput);
    }

    Super::MoveAutonomous(
        ClientTimeStamp,
        DeltaTime,
        CompressedFlags,
        NewAccel);
}

bool USPCharacterMovementComponent::ServerCheckClientError(
    float ClientTimeStamp,
    float DeltaTime,
    const FVector& Accel,
    const FVector& ClientWorldLocation,
    const FVector& RelativeClientLocation,
    FMovementBaseInterfaceData* ClientMovementBaseInterfaceData,
    FName ClientBaseBoneName,
    uint8 ClientMovementMode)
{
    if (IsCustomGravityMovement() && UpdatedComponent)
    {
        if (const FCharacterNetworkMoveData* BaseMoveData =
            GetCurrentNetworkMoveData())
        {
            const FSPNetworkMoveData* SPMoveData =
                static_cast<const FSPNetworkMoveData*>(BaseMoveData);

            const float RotationErrorDegrees =
                FMath::RadiansToDegrees(
                    UpdatedComponent->GetComponentQuat()
                    .AngularDistance(
                        SPMoveData->EndBodyRotation.Quaternion()));

            // 위치가 같아도 몸 자세가 다르면 다음 추진이 달라진다.
            // 클라이언트 자세를 신뢰해 적용하지 않고, 서버 계산과 비교해 보정 여부만 결정한다.
            if (RotationErrorDegrees > FMath::Clamp(NetworkBodyRotationTolerance, 0.1f, 45.0f))
            {
                return true;
            }
        }
    }

    return Super::ServerCheckClientError(
        ClientTimeStamp,
        DeltaTime,
        Accel,
        ClientWorldLocation,
        RelativeClientLocation,
        ClientMovementBaseInterfaceData,
        ClientBaseBoneName,
        ClientMovementMode);
}
