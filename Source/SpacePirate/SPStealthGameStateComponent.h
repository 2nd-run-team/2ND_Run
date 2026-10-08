#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPStealthTypes.h"
#include "SPStealthGameStateComponent.generated.h"

/** 전체 경보와 스테이지 상태의 유일한 원본. 기존 열차 GameState에 붙여 참가자에게 복제한다.
 * 열차 BP의 환경 기능은 유지하고, 부모는 프로젝트의 ASPGameState 계층을 사용한다.
 */
UCLASS(ClassGroup=(SpacePirate), meta=(BlueprintSpawnableComponent))
class SPACEPIRATE_API USPStealthGameStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPStealthGameStateComponent();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UFUNCTION(BlueprintPure, Category="Stealth") FSPStealthAlarmState GetAlarmState() const { return State; }
    /** 복제 수신 시에도 호출되는 표시 갱신 알림. 목표 진행 등 최초 경보 효과를 여기서 실행하지 않는다. */
    UPROPERTY(BlueprintAssignable, Category="Stealth") FSPStealthStateChanged OnStateChanged;
private:
    friend class USPGuardAlertSubsystem;
    UPROPERTY(ReplicatedUsing=OnRep_State) FSPStealthAlarmState State;
    UFUNCTION() void OnRep_State() { OnStateChanged.Broadcast(); }
    void Publish();
};
