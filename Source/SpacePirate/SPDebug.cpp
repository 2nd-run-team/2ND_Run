#include "SPDebug.h"

// 호출 매크로와 같은 조건으로 로그 카테고리 자체도 최종/테스트 빌드에서 제외한다.
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
DEFINE_LOG_CATEGORY(LogSpacePirate);
#endif
