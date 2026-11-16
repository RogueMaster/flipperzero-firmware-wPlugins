#pragma once

#include "../core/display.h"

typedef struct Backlight Backlight;

Backlight* backlight_alloc(DisplayLight light);

void backlight_free(Backlight* backlight);

void backlight_set(Backlight* backlight, DisplayLight light);
