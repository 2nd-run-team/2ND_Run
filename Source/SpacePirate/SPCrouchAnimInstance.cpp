#include "SPCrouchAnimInstance.h"
#include "GameFramework/Character.h"

void USPCrouchAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ACharacter* Character = Cast<ACharacter>(TryGetPawnOwner());
    CrouchAlpha = FMath::FInterpTo(CrouchAlpha, Character && Character->bIsCrouched ? 1.0f : 0.0f, DeltaSeconds, 14.0f);
}
