#pragma once

#include <input/input.h>
#include <furi.h>

void start_play_screen(void* data);

void render_play_screen(void* data);

void update_play_screen(void* data);

void input_play_screen(void* data, InputKey key, InputType type);
bool check_finish(void* data);
