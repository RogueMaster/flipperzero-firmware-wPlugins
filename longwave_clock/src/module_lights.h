#ifndef MODULE_LIGHTS_HEADERS
#define MODULE_LIGHTS_HEADERS

#include "flipper.h"
#include "app_state.h"

void lwc_app_backlight_on(App* app);
void lwc_app_backlight_on_persist(App* app);
void lwc_app_backlight_on_reset(App* app);
void lwc_app_led_on_receive_clear(App* app);
void lwc_app_led_on_receive_unknown(App* app);
void lwc_app_led_on_sync(App* app);
void lwc_app_led_on_desync(App* app);

#endif
