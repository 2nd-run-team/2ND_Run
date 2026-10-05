#include "SPGuardAlertSubsystem.h"
#include "SPGuardCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

bool USPGuardAlertSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool USPGuardAlertSubsystem::IsIdentified(FName Group, const APlayerState* Player) const
{
    const auto* Known = IdentifiedPlayers.Find(Group);
    return Player && Known && Known->Contains(TWeakObjectPtr<APlayerState>(const_cast<APlayerState*>(Player)));
}

void USPGuardAlertSubsystem::ReportSighting(ASPGuardCharacter* Witness, ASPPlayerCharacter* Player, const FVector& Location)
{
    if (!IsValid(Witness) || !Witness->HasAuthority() || !IsValid(Player)
        || !Player->IsPlayerControlled() || !Player->GetPlayerState() || Player->GetWorld() != GetWorld())
    {
        return;
    }
    IdentifiedPlayers.FindOrAdd(Witness->AlertGroup).Add(Player->GetPlayerState());
    for (TActorIterator<ASPGuardCharacter> It(GetWorld()); It; ++It)
    {
        if (It->AlertGroup == Witness->AlertGroup)
        {
            It->ReceiveSighting(Player, Location);
        }
    }
}
