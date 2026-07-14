#include "vk_thermo_led.h"
#include "../vk_thermo.h"

void vk_thermo_led_set_rgb(void* context, int red, int green, int blue) {
    VkThermo* app = context;
    if(app->led != 1) {
        return;
    }
    NotificationMessage notification_led_message_1;
    notification_led_message_1.type = NotificationMessageTypeLedRed;
    NotificationMessage notification_led_message_2;
    notification_led_message_2.type = NotificationMessageTypeLedGreen;
    NotificationMessage notification_led_message_3;
    notification_led_message_3.type = NotificationMessageTypeLedBlue;

    notification_led_message_1.data.led.value = red;
    notification_led_message_2.data.led.value = green;
    notification_led_message_3.data.led.value = blue;
    const NotificationSequence notification_sequence = {
        &notification_led_message_1,
        &notification_led_message_2,
        &notification_led_message_3,
        &message_do_not_reset,
        NULL,
    };
    notification_message(app->notification, &notification_sequence);
}

void vk_thermo_led_reset(void* context) {
    VkThermo* app = context;
    notification_message(app->notification, &sequence_reset_red);
    notification_message(app->notification, &sequence_reset_green);
    notification_message(app->notification, &sequence_reset_blue);
}

// Heartbeat "lub-dub" blue flash pattern for tag detection
void vk_thermo_led_detect(void* context) {
    VkThermo* app = context;
    if(app->led != 1) return;
    static const NotificationSequence seq_detect = {
        // "Lub" - first beat
        &message_blue_255,
        &message_delay_100,
        &message_blue_0,
        // Short gap between beats
        &message_delay_100,
        // "Dub" - second beat
        &message_blue_255,
        &message_delay_100,
        &message_blue_0,
        NULL,
    };
    notification_message(app->notification, &seq_detect);
}

// Non-blocking green flash
void vk_thermo_led_success(void* context) {
    VkThermo* app = context;
    if(app->led != 1) return;
    static const NotificationSequence seq_success = {
        &message_green_255,
        &message_delay_100,
        &message_green_0,
        NULL,
    };
    notification_message(app->notification, &seq_success);
}

// Non-blocking red flash
void vk_thermo_led_error(void* context) {
    VkThermo* app = context;
    if(app->led != 1) return;
    static const NotificationSequence seq_error = {
        &message_red_255,
        &message_delay_100,
        &message_red_0,
        NULL,
    };
    notification_message(app->notification, &seq_error);
}
