#pragma once

#include <gui/canvas.h>

#include "../core/action.h"
#include "../core/link.h"
#include "../core/mode.h"

void view_draw(Canvas* canvas, Mode mode, Link link, Action flash);
