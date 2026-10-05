#include "SPGuardAnimInstance.h"
#include "SPGuardCharacter.h"

void USPGuardAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ASPGuardCharacter* Guard = Cast<ASPGuardCharacter>(TryGetPawnOwner());
    GroundSpeed = Guard ? Guard->GetVelocity().Size2D() : 0;
    BlendSpeed = 0;
    Direction = 0;
    StridePlayRate = 1;
    if (!Guard || GroundSpeed < 3) { return; }

    const float WalkSpeed = FMath::Max(Guard->PatrolSpeed, 1.0f);
    const float RunSpeed = FMath::Max(Guard->ChaseSpeed, WalkSpeed + 1.0f);
    BlendSpeed = GroundSpeed <= WalkSpeed
        ? 300 * GroundSpeed / WalkSpeed
        : FMath::GetMappedRangeValueClamped(FVector2D(WalkSpeed, RunSpeed), FVector2D(300, 600), GroundSpeed);
    StridePlayRate = FMath::Clamp(GroundSpeed / FMath::Max(BlendSpeed, 1.0f), 0.1f, 2.0f);
    const FVector LocalVelocity = Guard->GetActorRotation().UnrotateVector(Guard->GetVelocity());
    Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
}
