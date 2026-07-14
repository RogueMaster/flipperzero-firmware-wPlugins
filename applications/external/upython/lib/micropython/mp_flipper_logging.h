#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MP_FLIPPER_LOG_LEVEL_TRACE (6)
#define MP_FLIPPER_LOG_LEVEL_DEBUG (5)
#define MP_FLIPPER_LOG_LEVEL_INFO (4)
#define MP_FLIPPER_LOG_LEVEL_WARN (3)
#define MP_FLIPPER_LOG_LEVEL_ERROR (2)
#define MP_FLIPPER_LOG_LEVEL_NONE (1)

uint8_t mp_flipper_log_get_effective_level();
void mp_flipper_log(uint8_t raw_level, const char* message);
