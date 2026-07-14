#include "vk_thermo_haptic.h"
#include "../vk_thermo.h"

// Non-blocking haptic using notification sequences

void vk_thermo_play_happy_bump(void* context) {
    VkThermo* app = context;
    if(app->haptic != 1) return;

    // Short 20ms vibration pulse
    static const NotificationSequence seq_happy = {
        &message_vibro_on,
        &message_delay_25,
        &message_vibro_off,
        NULL,
    };
    notification_message(app->notification, &seq_happy);
}

void vk_thermo_play_bad_bump(void* context) {
    VkThermo* app = context;
    if(app->haptic != 1) return;

    // Longer 100ms vibration pulse
    static const NotificationSequence seq_bad = {
        &message_vibro_on,
        &message_delay_100,
        &message_vibro_off,
        NULL,
    };
    notification_message(app->notification, &seq_bad);
}

void vk_thermo_play_long_bump(void* context) {
    VkThermo* app = context;
    if(app->haptic != 1) return;

    // Pulsing vibration pattern
    static const NotificationSequence seq_long = {
        &message_vibro_on,
        &message_delay_50,
        &message_vibro_off,
        &message_delay_100,
        &message_vibro_on,
        &message_delay_50,
        &message_vibro_off,
        NULL,
    };
    notification_message(app->notification, &seq_long);
}

void vk_thermo_play_heartbeat(void* context) {
    VkThermo* app = context;
    if(app->haptic != 1) return;

    // Heartbeat "lub-dub" vibration pattern
    static const NotificationSequence seq_heartbeat = {
        // "Lub" - first beat
        &message_vibro_on,
        &message_delay_100,
        &message_vibro_off,
        // Short gap between beats
        &message_delay_100,
        // "Dub" - second beat
        &message_vibro_on,
        &message_delay_100,
        &message_vibro_off,
        NULL,
    };
    notification_message(app->notification, &seq_heartbeat);
}
