#pragma once

// NOTICE [TEMP-GRAVITY-SWITCH]: 정식 장치 도입 후 이 클래스와 캐릭터의 버튼 검색/RPC를 교체한다.
// 영역/Subsystem은 유지한다. Shipping/Test에서는 숨기고 충돌과 사용을 차단한다.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPGravitySwitch.generated.h"

class ASPGravityZone;
class UStaticMeshComponent;

/** 작업자: 김세훈 | 서버 검증을 통과한 상호작용으로 연결된 영역을 전환하는 임시 버튼. */
UCLASS()
class SPACEPIRATE_API ASPGravitySwitch : public AActor
{
    GENERATED_BODY()
public:
    /** 작업자: 김세훈 | 시선 검색에만 반응하는 버튼 메시를 생성한다. */
    ASPGravitySwitch();
    /** 작업자: 김세훈 | 캐릭터 RPC의 거리/시선 검사를 거친 뒤 서버에서 호출한다. */
    bool TryActivate();
protected:
    /** 작업자: 김세훈 | 설정 누락 진단 및 Shipping/Test 비활성화. */
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> ButtonMesh;
    /** 레벨의 버튼 인스턴스에서 제어할 영역을 선택한다. */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gravity")
    TObjectPtr<ASPGravityZone> TargetGravityZone;
};
