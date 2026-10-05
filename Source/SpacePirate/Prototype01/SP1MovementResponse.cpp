// 작성자 : 임진혁
#include "Prototype01/SP1MovementResponse.h"
#include "Prototype01/SP1SurvivalComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"

void FSP1MovementResponse::ServerFillResponseData(const UCharacterMovementComponent& Movement, const FClientAdjustment& Adjustment)
{
    FCharacterMoveResponseDataContainer::ServerFillResponseData(Movement, Adjustment);
    const auto* Life = Movement.GetCharacterOwner()->FindComponentByClass<USP1SurvivalComponent>();
    bHasSurvival = Life != nullptr;
    if (Life) { Stamina = Life->GetMovementState(); Wounds = Life->Wounds; }
}
bool FSP1MovementResponse::Serialize(UCharacterMovementComponent& Movement, FArchive& Ar, UPackageMap* Map)
{
    const bool bOK = FCharacterMoveResponseDataContainer::Serialize(Movement, Ar, Map);
    Ar.SerializeBits(&bHasSurvival, 1);
    if (bHasSurvival)
    {
        Ar << Stamina.Current << Stamina.RecoveryWait << Wounds;
        Ar.SerializeBits(&Stamina.bExhausted, 1);
    }
    return bOK && !Ar.IsError();
}
