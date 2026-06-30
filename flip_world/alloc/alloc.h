#pragma once
#include <flip_world.h>
FlipWorldApp *flip_world_app_alloc();
void flip_world_app_free(FlipWorldApp *app);
void flip_world_show_submenu();
uint32_t callback_exit_app(void *context);
uint32_t callback_to_submenu(void *context);
uint32_t callback_to_wifi_settings(void *context);
uint32_t callback_to_settings(void *context);