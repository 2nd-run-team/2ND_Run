// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "Prototype01/SP1TransferCargo.h"
#include "SP1GravityCargo.generated.h"
class ASPGravityZone;
/** S01 전송과 객차 중력 상태를 동일한 서버 완료 지점에서 변경한다. */
UCLASS()
class SPACEPIRATE_API ASP1GravityCargo : public ASP1TransferCargo
{
    GENERATED_BODY()
public:
    ASP1GravityCargo();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") TObjectPtr<ASPGravityZone> GravityZone;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UStaticMeshComponent>> RingSegments;
    virtual void SetTransferred(bool bNewTransferred) override;
protected:
    virtual void OnRep_Transferred() override;
};
