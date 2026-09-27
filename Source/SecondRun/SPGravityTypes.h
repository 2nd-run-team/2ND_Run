#pragma once

#include "CoreMinimal.h"
#include "SPGravityTypes.generated.h"

// 게임 규칙에서 사용하는 중력 상태 이름. 캐릭터의 실제 상태는 MovementMode에서 파생한다.
// 이 enum 자체가 상태를 복제하거나 화물에 물리를 적용하지는 않는다.
UENUM(BlueprintType)
enum class ESPGravityMode : uint8
{
    Gravity     UMETA(DisplayName = "Gravity"),
    ZeroGravity UMETA(DisplayName = "Zero Gravity")
};
