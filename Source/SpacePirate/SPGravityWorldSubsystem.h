#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SPGravityTypes.h"
#include "SPGravityWorldSubsystem.generated.h"

class ASPGravityZone;

/** 작업자: 김세훈 | 서버의 영역 참조 목록과 위치 조회를 관리한다. 상태 원본은 각 영역에 있다. */
UCLASS()
class SPACEPIRATE_API USPGravityWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** 작업자: 김세훈 | 같은 월드의 서버 영역을 중복 없이 등록한다. */
    void RegisterZone(ASPGravityZone* Zone);
    /** 작업자: 김세훈 | 사라진 영역의 약한 참조도 함께 정리한다. */
    void UnregisterZone(ASPGravityZone* Zone);
    /** 작업자: 김세훈 | 위치를 포함하는 최고 우선순위 영역을 찾는다. */
    ASPGravityZone* FindZoneAtLocation(const FVector& WorldLocation) const;
    /** 작업자: 김세훈 | 영역이 없으면 기본 중력을 반환한다. 서버에서 사용한다. */
    ESPGravityMode GetGravityModeAtLocation(const FVector& WorldLocation) const;
protected:
    /** 작업자: 김세훈 | 게임/PIE 월드에서만 생성한다. */
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    TArray<TWeakObjectPtr<ASPGravityZone>> Zones;
};
