#ifndef FLIP_WEATHER_CALLBACK_H
#define FLIP_WEATHER_CALLBACK_H

#include "flip_weather.h"
#include <parse/flip_weather_parse.h>
#include <flip_storage/flip_weather_storage.h>

extern bool weather_request_success;
extern bool sent_weather_request;
extern bool got_weather_data;

void flip_weather_view_draw_callback_weather(Canvas *canvas, void *model);
void flip_weather_view_draw_callback_gps(Canvas *canvas, void *model);
void temperature_unit_change(VariableItem *item);
void callback_submenu_choices(void *context, uint32_t index);
void text_updated_ssid(void *context);
void text_updated_password(void *context);
void text_updated_location(void *context);
uint32_t callback_to_submenu(void *context);
void settings_item_selected(void *context, uint32_t index);

/**
 * @brief Navigation callback for exiting the application
 * @param context The context - unused
 * @return next view id (VIEW_NONE to exit the app)
 */
uint32_t callback_exit_app(void *context);
uint32_t callback_to_wifi_settings(void *context);

// Add edits by Derek Jamison
void flip_weather_generic_switch_to_view(FlipWeatherApp *app, char *title, DataLoaderFetch fetcher, DataLoaderParser parser, size_t request_count, ViewNavigationCallback back, uint32_t view_id);

void flip_weather_loader_draw_callback(Canvas *canvas, void *model);

void flip_weather_loader_init(View *view);

void flip_weather_loader_free_model(View *view);

bool flip_weather_custom_event_callback(void *context, uint32_t index);
#endif