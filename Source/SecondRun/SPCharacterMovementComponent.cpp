#include "SPCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementReplication.h"

// ------------------------------------------------------------
// 클라이언트 이동 기록
// ------------------------------------------------------------

class FSavedMove_SP final : public FSavedMove_Character
{
public:
    using Super = FSavedMove_Character;

    bool bSavedSprintRequested = false;

    virtual void Clear() override
    {
        Super::Clear();

        bSavedSprintRequested = false;
    }

    virtual uint8 GetCompressedFlags() const override
    {
        uint8 Flags = Super::GetCompressedFlags();

        if (bSavedSprintRequested)
        {
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

        // 달리기 입력이 다른 이동 기록은 합치지 않는다.
        if (bSavedSprintRequested != NewSPMove->bSavedSprintRequested)
        {
            return false;
        }

        // 전진 방향 판정에 시선을 사용하므로,
        // 회전이 다른 기록도 분리한다.
        if (!StartControlRotation.Equals(
            NewSPMove->StartControlRotation, 0.01f))
        {
            return false;
        }

        return Super::CanCombineWith(
            NewMove, Character, MaxDelta);
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

        bSavedSprintRequested = Movement->IsSprintRequested();
    }

    virtual void PrepMoveFor(ACharacter* Character) override
    {
        Super::PrepMoveFor(Character);

        USPCharacterMovementComponent* Movement =
            CastChecked<USPCharacterMovementComponent>(
                Character->GetCharacterMovement());

        Movement->SetSprintRequested(bSavedSprintRequested);
    }
};

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
        return FSavedMovePtr(new FSavedMove_SP());
    }
};

// ------------------------------------------------------------
// 이동 컴포넌트
// ------------------------------------------------------------

USPCharacterMovementComponent::USPCharacterMovementComponent()
{
    MaxWalkSpeed = 400.0f;
    SprintSpeed = 700.0f;

    MaxAcceleration = 2048.0f;
    BrakingDecelerationWalking = 2048.0f;
    GroundFriction = 8.0f;

    JumpZVelocity = 420.0f;
    GravityScale = 1.0f;

    // 이번 버전은 공중 방향 조작을 사용하지 않는다.
    AirControl = 0.0f;
    FallingLateralFriction = 0.0f;
    BrakingDecelerationFalling = 0.0f;

    bOrientRotationToMovement = false;
    bUseControllerDesiredRotation = false;
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
    return ForwardDot > 0.5f;
}

float USPCharacterMovementComponent::GetMaxSpeed() const
{
    const float EffectiveSprintSpeed =
        FMath::Max(MaxWalkSpeed, SprintSpeed);

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
}

FNetworkPredictionData_Client*
USPCharacterMovementComponent::GetPredictionData_Client() const
{
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
    // 과거 이동을 재생해도 현재 프레임의 입력을 보존한다.
    const bool bCurrentSprintRequest = bSprintRequested;

    const bool bUpdated =
        Super::ClientUpdatePositionAfterServerUpdate();

    bSprintRequested = bCurrentSprintRequest;

    return bUpdated;
}