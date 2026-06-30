#pragma once

#include <strings.h> // for size_t
#include "../icon.h"

void canvas_initialize(IEIcon* icon, size_t scale);
void canvas_free_canvas();

void canvas_set_scale(size_t scale_setting);
