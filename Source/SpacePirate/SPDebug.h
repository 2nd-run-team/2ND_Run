#pragma once

#include "CoreMinimal.h"

// 프로젝트 공통 진단 정책: 조치가 필요한 Warning/Error만 출력한다.
// 정상 입력/일반 충돌/모드 전환/매 프레임 수치는 기록하지 않는다.
// Shipping/Test에서는 호출 인수 평가까지 제거하므로, 인수에 게임 상태를 바꾸는 코드를 넣지 않는다.
// 이 진단 기반은 임시 기능이 아니다. 테스트 키 제거 후에도 유지한다.
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
DECLARE_LOG_CATEGORY_EXTERN(LogSpacePirate, Warning, All);
#define SP_DEBUG_LOG(Verbosity, ...) UE_LOG(LogSpacePirate, Verbosity, __VA_ARGS__)
#else
#define SP_DEBUG_LOG(Verbosity, ...) do {} while (false)
#endif
