#pragma once

#include "ui_internal.h"

void camera_center_on_player(AppContext* app);
void camera_update(AppContext* app);
void clamp_cursor_to_view(AppContext* app);
void cursor_move(AppContext* app, int8_t dx, int8_t dy);
