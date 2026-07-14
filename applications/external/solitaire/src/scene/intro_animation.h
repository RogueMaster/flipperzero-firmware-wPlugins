#pragma once
#include <furi.h>
#include <input/input.h>

void start_intro_screen(void* data);

void render_intro_screen(void* data);

void update_intro_screen(void* data);

void input_intro_screen(void* data, InputKey key, InputType type);
