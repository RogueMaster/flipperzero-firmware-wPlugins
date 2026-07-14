#pragma once

#include "mouse_click_recorder.h"

void hid_init(App* app);
void hid_deinit(App* app);
void hid_ensure_init(App* app);
void hid_mouse_move(App* app, int8_t dx, int8_t dy);
void hid_mouse_press(App* app);
void hid_mouse_release(App* app);
void hid_mouse_click(App* app);
bool hid_is_connected(App* app);
void hid_mouse_press_btn(App* app, uint8_t btn);
void hid_mouse_release_btn(App* app, uint8_t btn);
void hid_mouse_release_all(App* app);
