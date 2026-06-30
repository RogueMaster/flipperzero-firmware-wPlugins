#pragma once
#include <furi.h>
#include <input/input.h>

void start_falling_screen(void* data);

void render_falling_screen(void* data);

void update_falling_screen(void* data);

void input_falling_screen(void* data, InputKey key, InputType type);
