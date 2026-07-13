/* Host shim of <furi_hal_rtc.h>. The implementation lives in the test runner. */
#pragma once

#include <stdint.h>

uint32_t furi_hal_rtc_get_timestamp(void);
