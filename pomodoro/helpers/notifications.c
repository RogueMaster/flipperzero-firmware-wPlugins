#include "notifications.h"

const NotificationSequence stop_all_notification = {
    &message_sound_off,
    &message_vibro_off,

    &message_red_0,
    &message_green_0,
    &message_blue_0,
    
    NULL,
};

const NotificationSequence flash_backlight_on_sequence = {
    &message_display_backlight_on,
    &message_green_255,
    NULL,
};

const NotificationSequence flash_backlight_off_sequence = {
    &message_display_backlight_off,
    &message_green_0,
    NULL,
};

const NotificationSequence vibrate_sequence = {
    &message_vibro_on,
    &message_delay_250,
    &message_vibro_off,
    NULL,
};

const NotificationSequence soft_beep_sequence = {
    &message_display_backlight_on,
    &message_note_d5,
    &message_delay_250,
    &message_sound_off,
    NULL,
};

const NotificationSequence loud_beep_sequence = {
    &message_display_backlight_on,
    &message_note_d5,
    &message_delay_250,
    &message_note_b5,
    &message_delay_250,
    &message_note_d5,
    &message_delay_250,
    &message_sound_off,
    NULL,
};

const NotificationSequence work_start_notification = {
    &message_display_backlight_on,

    &message_vibro_on,

    &message_note_b5,
    &message_delay_250,
    &message_delay_100,

    &message_note_d5,
    &message_delay_250,
    &message_delay_100,

    &message_sound_off,
    &message_vibro_off,

    &message_green_255,
    &message_delay_1000,
    &message_green_0,
    &message_delay_250,
    &message_green_255,
    &message_delay_1000,
    

    NULL,
};

const NotificationSequence rest_start_notification = {
    &message_display_backlight_on,

    &message_vibro_on,

    &message_note_d5,
    &message_delay_250,

    &message_note_b5,
    &message_delay_250,

    &message_sound_off,
    &message_vibro_off,

    &message_red_255,
    &message_delay_1000,
    &message_red_0,
    &message_delay_250,
    &message_red_255,
    &message_delay_1000,

    NULL,
};


// Dolphin laughing sound
const NotificationSequence long_break_start_notification = {
    &message_display_backlight_on,

    &message_vibro_on,

    &message_note_d3,
    &message_delay_50,
    &message_note_d5,
    &message_delay_100,
    &message_note_d7,
    &message_delay_250,
    &message_sound_off,
    &message_delay_250,

    &message_note_d3,
    &message_delay_100,
    &message_note_d5,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,

    &message_note_d3,
    &message_delay_100,
    &message_note_d5,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,

    &message_note_d3,
    &message_delay_100,
    &message_note_d5,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,

    &message_note_d3,
    &message_delay_100,
    &message_note_d5,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,

    &message_note_d3,
    &message_delay_100,
    &message_note_d5,
    &message_delay_250,
    &message_note_d7,
    &message_delay_500,

    &message_sound_off,
    &message_vibro_off,

    &message_blue_255,
    &message_delay_1000,
    &message_blue_0,
    &message_delay_250,
    &message_blue_255,
    &message_delay_1000,

    NULL,
};

const NotificationSequence* stage_start_notification_sequence_map[] = {
    [FlippPomodoroStageFocus] = &work_start_notification,
    [FlippPomodoroStageRest] = &rest_start_notification,
    [FlippPomodoroStageLongBreak] = &long_break_start_notification,
};
