#pragma once
#include <furi.h>
#include <furi_hal_version.h>

// Check if we're building for Momentum firmware
#if defined(FW_ORIGIN_Momentum)
#include <momentum/momentum.h>
#define HAS_MOMENTUM_SUPPORT 1
#endif
