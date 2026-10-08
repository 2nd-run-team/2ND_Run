#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPStealthTypes.h"
#include "SPStealthPlayerStateComponent.generated.h"

/** 플레이어 신원 상태의 유일한 원본. 기존 PlayerState에 붙어 모든 참가자에게 복제된다.
 * Pawn의 사망·교체와 신원 초기화를 분리하며, 발각 기록은 명시적인 스테이지 초기화로 지운다.
 */
UCLASS(ClassGroup=(SpacePirate), meta=(BlueprintSpawnableComponent))
class SPACEPIRATE_API USPStealthPlayerStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPStealthPlayerStateComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UFUNCTION(BlueprintPure, Category="Stealth") FSPStealthIdentityState GetIdentityState() const { return State; }
    UFUNCTION(BlueprintPure, Category="Stealth") bool IsIdentified() const { return !State.KnownToScopes.IsEmpty(); }
    UFUNCTION(BlueprintPure, Category="Stealth") bool IsKnownTo(FName Scope) const { return State.KnownToScopes.Contains(Scope); }
    /** UI 갱신용 알림. 최초 발각에 따른 게임 효과는 서버 Subsystem의 최초 발생 이벤트에 연결한다. */
    UPROPERTY(BlueprintAssignable, Category="Stealth") FSPStealthStateChanged OnStateChanged;
private:
    friend class USPGuardAlertSubsystem;
    UPROPERTY(ReplicatedUsing=OnRep_State) FSPStealthIdentityState State;
    UFUNCTION() void OnRep_State() { OnStateChanged.Broadcast(); }
    void Publish();
};
