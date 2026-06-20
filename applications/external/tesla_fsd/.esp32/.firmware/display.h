#pragma once
#include "fsd_handler.h"

void display_init();
void display_update(const FSDState *state);
void display_set_enabled(bool enabled);
void display_wake();
void display_sleep();
void display_set_brightness(uint8_t percentage);
bool display_is_awake();
