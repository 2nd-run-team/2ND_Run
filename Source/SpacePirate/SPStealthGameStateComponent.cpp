#include "SPStealthGameStateComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
USPStealthGameStateComponent::USPStealthGameStateComponent() { SetIsReplicatedByDefault(true); }
void USPStealthGameStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USPStealthGameStateComponent, State);
}
void USPStealthGameStateComponent::Publish() { GetOwner()->ForceNetUpdate(); OnRep_State(); }

