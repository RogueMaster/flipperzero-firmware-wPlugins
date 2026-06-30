#pragma once
#include <flip_world.h>

void callback_submenu_choices(void *context, uint32_t index);
bool callback_message_input(InputEvent *event, void *context);
void callback_message_draw(Canvas *canvas, void *model);
void callback_wifi_settings_select(void *context, uint32_t index);
void callback_updated_wifi_ssid(void *context);
void callback_updated_wifi_pass(void *context);
void callback_updated_username(void *context);
void callback_updated_password(void *context);
void callback_fps_change(VariableItem *item);
void callback_game_settings_select(void *context, uint32_t index);
void callback_user_settings_select(void *context, uint32_t index);
void callback_screen_on_change(VariableItem *item);
void callback_sound_on_change(VariableItem *item);
void callback_vibration_on_change(VariableItem *item);
void callback_player_on_change(VariableItem *item);
void callback_vgm_x_change(VariableItem *item);
void callback_vgm_y_change(VariableItem *item);
void callback_submenu_lobby_pvp_choices(void *context, uint32_t index);