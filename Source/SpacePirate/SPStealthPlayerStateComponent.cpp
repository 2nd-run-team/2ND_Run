#include "SPStealthPlayerStateComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
USPStealthPlayerStateComponent::USPStealthPlayerStateComponent() { SetIsReplicatedByDefault(true); }
void USPStealthPlayerStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USPStealthPlayerStateComponent, State);
}
void USPStealthPlayerStateComponent::Publish() { GetOwner()->ForceNetUpdate(); OnRep_State(); }

