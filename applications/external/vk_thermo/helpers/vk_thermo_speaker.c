#include "vk_thermo_speaker.h"
#include "../vk_thermo.h"

// Non-blocking sound using notification sequences
// The notification service handles timing and speaker cleanup internally

void vk_thermo_play_detect_sound(void* context) {
    VkThermo* app = context;
    if(app->speaker != 1) return;

    // Short chirp at E5 (660Hz)
    static const NotificationMessage msg_detect = {
        .type = NotificationMessageTypeSoundOn,
        .data.sound.frequency = NOTE_DETECT,
        .data.sound.volume = 0.8f,
    };
    static const NotificationSequence seq_detect = {
        &msg_detect,
        &message_delay_50,
        &message_sound_off,
        NULL,
    };
    notification_message(app->notification, &seq_detect);
}

void vk_thermo_play_success_sound(void* context) {
    VkThermo* app = context;
    if(app->speaker != 1) return;

    // Longer tone at A5 (880Hz)
    static const NotificationMessage msg_success = {
        .type = NotificationMessageTypeSoundOn,
        .data.sound.frequency = NOTE_SUCCESS,
        .data.sound.volume = 1.0f,
    };
    static const NotificationSequence seq_success = {
        &msg_success,
        &message_delay_100,
        &message_sound_off,
        NULL,
    };
    notification_message(app->notification, &seq_success);
}

void vk_thermo_play_error_sound(void* context) {
    VkThermo* app = context;
    if(app->speaker != 1) return;

    // Low tone at A3 (220Hz)
    static const NotificationMessage msg_error = {
        .type = NotificationMessageTypeSoundOn,
        .data.sound.frequency = NOTE_ERROR,
        .data.sound.volume = 1.0f,
    };
    static const NotificationSequence seq_error = {
        &msg_error,
        &message_delay_100,
        &message_sound_off,
        NULL,
    };
    notification_message(app->notification, &seq_error);
}

void vk_thermo_stop_all_sound(void* context) {
    VkThermo* app = context;
    if(app->speaker != 1) return;

    static const NotificationSequence seq_stop = {
        &message_sound_off,
        NULL,
    };
    notification_message(app->notification, &seq_stop);
}
