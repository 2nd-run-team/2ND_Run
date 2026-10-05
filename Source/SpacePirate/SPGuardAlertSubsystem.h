#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SPGuardAlertSubsystem.generated.h"

class APlayerState;
class ASPPlayerCharacter;
class ASPGuardCharacter;

/** 서버 전용 발각 신원 기억. 위치 신호는 목격한 시점에만 전송한다. */
UCLASS()
class SPACEPIRATE_API USPGuardAlertSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    bool IsIdentified(FName Group, const APlayerState* Player) const;
    void ReportSighting(ASPGuardCharacter* Witness, ASPPlayerCharacter* Player, const FVector& Location);
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    TMap<FName, TSet<TWeakObjectPtr<APlayerState>>> IdentifiedPlayers;
};
