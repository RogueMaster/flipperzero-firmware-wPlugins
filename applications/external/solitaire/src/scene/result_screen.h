#pragma once
#include <furi.h>
#include <input/input.h>

void start_result_screen(void* data);

void render_result_screen(void* data);

void update_result_screen(void* data);

void input_result_screen(void* data, InputKey key, InputType type);
