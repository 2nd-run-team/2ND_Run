#pragma once

#include "CoreMinimal.h"

// Report actionable warnings/errors only; add routine telemetry only on request.
// Compile project diagnostics out even if Shipping logging is enabled elsewhere.
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
DECLARE_LOG_CATEGORY_EXTERN(LogSpacePirate, Warning, All);
#define SP_DEBUG_LOG(Verbosity, ...) UE_LOG(LogSpacePirate, Verbosity, __VA_ARGS__)
#else
#define SP_DEBUG_LOG(Verbosity, ...) do {} while (false)
#endif
