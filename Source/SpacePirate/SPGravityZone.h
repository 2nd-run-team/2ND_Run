#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPGravityTypes.h"
#include "SPGravityZone.generated.h"

class UBoxComponent;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPGravityZoneChanged, ESPGravityMode, NewMode);

/**
 * 작업자: 김세훈
 * 범위와 현재 중력을 관리한다. 실제 이동/물리는 각 대상이 적용한다.
 * 영역 밖 기본값은 중력이며 중력 방향은 월드 아래 방향이다.
 */
UCLASS()
class SPACEPIRATE_API ASPGravityZone : public AActor
{
    GENERATED_BODY()
public:
    /** 작업자: 김세훈 | 복제 설정과 편집 가능한 범위를 생성한다. */
    ASPGravityZone();
    /** 작업자: 김세훈 | 늦게 접속한 클라이언트에도 현재 상태를 전달한다. */
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    /** 작업자: 김세훈 | 표시/UI 및 서버 위치 조회에서 사용할 현재 상태. */
    UFUNCTION(BlueprintPure, Category = "Gravity")
    ESPGravityMode GetGravityMode() const { return CurrentGravityMode; }
    /** 작업자: 김세훈 | 서버만 상태를 변경하며 같은 값은 재적용하지 않는다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Gravity")
    bool SetGravityMode(ESPGravityMode NewMode);
    /** 작업자: 김세훈 | 여러 버튼의 동시 사용도 영역 단위 쿨다운으로 제한한다. */
    bool TryToggleGravity();
    /** 작업자: 김세훈 | 회전/스케일을 반영한 박스 포함 여부를 검사한다. */
    bool ContainsPoint(const FVector& WorldLocation) const;
    /** 작업자: 김세훈 | 영역이 겹칠 때 높은 값을 우선한다. */
    int32 GetPriority() const { return Priority; }

    // 표시용 알림. 클라이언트 이동 모드를 강제로 바꾸는 데 사용하지 않는다.
    UPROPERTY(BlueprintAssignable, Category = "Gravity")
    FSPGravityZoneChanged OnGravityModeChanged;

protected:
    /** 작업자: 김세훈 | 초기 상태를 적용하고 서버의 영역 목록에 등록한다. */
    virtual void BeginPlay() override;
    /** 작업자: 김세훈 | 파괴/스트리밍 해제 시 목록에서 제거한다. */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 판정 범위. 에디터의 Box Extent와 Transform으로 조절한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> Bounds;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
    ESPGravityMode InitialGravityMode = ESPGravityMode::Gravity;
    /** 높은 값이 우선한다. 동일하면 액터 경로 이름 순서로 선택한다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity")
    int32 Priority = 0;
    /** 버튼 연타/동시 사용에 의한 즉시 재전환 방지 시간(초). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gravity", meta = (ClampMin = "0.0"))
    float SwitchCooldown = 0.25f;

private:
    UPROPERTY(ReplicatedUsing = OnRep_GravityMode)
    ESPGravityMode CurrentGravityMode = ESPGravityMode::Gravity;
    /** 작업자: 김세훈 | 복제된 상태를 표시용 구독자에게 알린다. */
    UFUNCTION()
    void OnRep_GravityMode();
    double LastSwitchTime = -1.0e30;
};
