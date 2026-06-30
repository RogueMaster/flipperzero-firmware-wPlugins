#pragma once

#include <furi.h>
#include <input/input.h>

#include "../../game_state.h"
#include "../../assets.h"
#include "../util/helpers.h"

void start_main_screen(void* data);

void render_main_screen(void* data);

void update_main_screen(void* data);

void input_main_screen(void* data, InputKey key, InputType type);
