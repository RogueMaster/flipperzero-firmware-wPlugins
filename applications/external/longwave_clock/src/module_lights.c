#include "module_lights.h"

void lwc_app_backlight_on(App* app) {
    if(!app->state->display_on) {
        notification_message_block(app->notifications, &sequence_display_backlight_on);
    }
}

void lwc_app_backlight_on_persist(App* app) {
    if(!app->state->display_on) {
        app->state->display_on = true;
        notification_message_block(app->notifications, &sequence_display_backlight_enforce_on);
    }
}

void lwc_app_backlight_on_reset(App* app) {
    if(app->state->display_on) {
        app->state->display_on = false;
        notification_message_block(app->notifications, &sequence_display_backlight_enforce_auto);
    }
}

void lwc_app_led_on_receive_clear(App* app) {
    notification_message_block(app->notifications, &sequence_blink_blue_10);
}

void lwc_app_led_on_receive_unknown(App* app) {
    notification_message_block(app->notifications, &sequence_blink_yellow_100);
}

void lwc_app_led_on_sync(App* app) {
    notification_message_block(app->notifications, &sequence_blink_green_100);
}

void lwc_app_led_on_desync(App* app) {
    notification_message_block(app->notifications, &sequence_blink_red_100);
}
