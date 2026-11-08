#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "action.h"

bool feedback_vibrates(Action action);

uint32_t feedback_flash_ms(Action action);
