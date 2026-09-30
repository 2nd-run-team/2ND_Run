#include "SPGravityWorldSubsystem.h"
#include "SPGravityZone.h"

bool USPGravityWorldSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USPGravityWorldSubsystem::RegisterZone(ASPGravityZone* Zone)
{
    if (IsValid(Zone) && Zone->HasAuthority() && Zone->GetWorld() == GetWorld())
    {
        Zones.AddUnique(TWeakObjectPtr<ASPGravityZone>(Zone));
    }
}

void USPGravityWorldSubsystem::UnregisterZone(ASPGravityZone* Zone)
{
    Zones.RemoveAll([Zone](const TWeakObjectPtr<ASPGravityZone>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Zone;
    });
}

ASPGravityZone* USPGravityWorldSubsystem::FindZoneAtLocation(const FVector& WorldLocation) const
{
    ASPGravityZone* Best = nullptr;
    for (const TWeakObjectPtr<ASPGravityZone>& Entry : Zones)
    {
        ASPGravityZone* Zone = Entry.Get();
        if (!IsValid(Zone) || !Zone->ContainsPoint(WorldLocation))
        {
            continue;
        }
        if (!Best || Zone->GetPriority() > Best->GetPriority()
            || (Zone->GetPriority() == Best->GetPriority()
                && Zone->GetPathName().Compare(Best->GetPathName(), ESearchCase::CaseSensitive) < 0))
        {
            Best = Zone;
        }
    }
    return Best;
}

ESPGravityMode USPGravityWorldSubsystem::GetGravityModeAtLocation(const FVector& WorldLocation) const
{
    const ASPGravityZone* Zone = FindZoneAtLocation(WorldLocation);
    return Zone ? Zone->GetGravityMode() : ESPGravityMode::Gravity;
}
