#pragma once

// 역할: 금고 안의 전리품 묶음(MVP안 11장). E를 2초 누르면 서버가 이 자리에 전리품 가방을 만들고 묶음은 사라진다.
// 인벤토리 물건이 아니라 줍지 못한다. 한 번에 한 명만 포장하고(상호작용 규칙) 완료되면 묶음이 사라지므로,
// 두 사람이 같은 묶음을 포장해도 가방은 하나만 나온다.
// NOTICE [LOOT-BUNDLE]: 금고가 생기면 "금고가 열려야 포장 가능"(10장) 조건을 연결한다. 포장 중 목격은 CrimeKind로 등록한다.

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPLootBundle.generated.h"

class APawn;
class ASPCargo;
class UStaticMeshComponent;
class USPInteractableComponent;

UCLASS()
class SPACEPIRATE_API ASPLootBundle : public AActor
{
    GENERATED_BODY()

public:
    ASPLootBundle();

    USPInteractableComponent* GetInteractable() const { return Interactable; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Loot")
    TObjectPtr<UStaticMeshComponent> BundleMesh;

    /** E 포장. 시간 2초(MVP안 11장)는 Details에서 조정한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
    TObjectPtr<USPInteractableComponent> Interactable;

    /** 포장이 끝나면 만들 가방. BP에서 BP_SPLootBag을 지정한다. 가치는 가방 BP의 Cargo Value를 따른다. */
    UPROPERTY(EditAnywhere, Category = "Loot")
    TSubclassOf<ASPCargo> LootBagClass;

private:
    void HandlePacked(APawn* User);
};
