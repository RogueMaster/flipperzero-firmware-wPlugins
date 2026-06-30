#pragma once
#include <flip_wifi.h>
#include <flip_storage/flip_wifi_storage.h>
#include <flip_wifi_icons.h>

uint32_t callback_exit_app(void *context);
uint32_t callback_submenu_ap(void *context);
uint32_t callback_to_submenu_main(void *context);
uint32_t callback_to_submenu_scan(void *context);
uint32_t callback_to_submenu_saved(void *context);
void callback_custom_command_updated(void *context);
void callback_ap_ssid_updated(void *context);
void callback_text_updated_deauth(void *context);
void callback_redraw_submenu_saved(void *context);
void callback_view_draw_callback_scan(Canvas *canvas, void *model);
void callback_view_draw_callback_saved(Canvas *canvas, void *model);
void callback_view_draw_callback_deauth(Canvas *canvas, void *model);
bool callback_view_input_callback_scan(InputEvent *event, void *context);
bool callback_view_input_callback_deauth(InputEvent *event, void *context);
bool callback_view_input_callback_saved(InputEvent *event, void *context);
void callback_timer_callback(void *context);
void callback_submenu_choices(void *context, uint32_t index);
void callback_text_updated_password_scan(void *context);
void callback_text_updated_password_saved(void *context);
void callback_text_updated_add_ssid(void *context);
void callback_text_updated_add_password(void *context);