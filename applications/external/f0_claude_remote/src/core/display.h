#pragma once

#include <stdbool.h>

#include "mode.h"

typedef enum {
    DisplayLightOn,
    DisplayLightAuto,
    DisplayLightOff,
} DisplayLight;

DisplayLight display_light(Mode mode, bool backlight);
