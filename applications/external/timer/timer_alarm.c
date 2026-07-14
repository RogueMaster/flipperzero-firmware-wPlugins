#include "timer_alarm.h"
#include "notification/notification_messages.h" // for message_* references
#include "notification/notification_messages_notes.h" // for message_note_* references

#ifndef ALARM_TYPE_NONE
#define ALARM_TYPE_NONE 0
#endif

// Revised time-up sequence with manually controlled fast blue LED blinking.
// This sequence forces display brightness/backlight on, activates vibro,
// then in each cycle it turns the LED on with message_blue_255, waits briefly,
// plays a beep using message_note_c8, then turns the LED off with message_blue_0.
// This cycle is repeated 4 times, and then vibro/backlight are turned off.
static const NotificationSequence sequence_timeup = {
    &message_force_display_brightness_setting_1f,
    &message_vibro_on,

    // 1st cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 2nd cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 3rd cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 4th cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    &message_delay_250,

    // 1st cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 2nd cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 3rd cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 4th cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    &message_delay_250,
    // 1st cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 2nd cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 3rd cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    // 4th cycle
    &message_display_backlight_on,
    &message_blue_255,
    &message_delay_10,
    &message_note_c8,
    &message_delay_50,
    &message_sound_off,
    &message_delay_10,
    &message_blue_0,
    &message_delay_10,
    &message_display_backlight_off,

    &message_delay_250,

    &message_vibro_off,
    &message_delay_250,
    NULL,
};

void stop_alarm_sound(PomodoroApp* app) {
    if(app->alarm_sound_active) {
        app->alarm_sound_active = false;
        furi_hal_speaker_stop();
        furi_delay_ms(10);
        if(furi_hal_speaker_is_mine()) {
            furi_hal_speaker_release();
        }
    }
}

void start_screen_blink(PomodoroApp* app, uint32_t duration_ms) {
    (void)duration_ms; // avoid -Wunused-parameter

    // 1. Display a dialog
    show_dialog(app, "Timer Alarm", "Time's Up!", 5000, ALARM_TYPE_NONE);

    // 2. Run the time-up sequence with fast blue LED blinking and beeps.
    NotificationApp* notif = furi_record_open("notification");
    notification_message(notif, &sequence_timeup);

    // Wait for the sequence to complete (~1000ms should be sufficient)
    furi_delay_ms(1000);

    // Close the notification safely
    furi_record_close("notification");
}

void toggle_backlight_with_vibro(PomodoroApp* app) {
    NotificationApp* notif = furi_record_open("notification");
    if(app->backlight_on) {
        notification_message(notif, &sequence_display_backlight_off);
        app->backlight_on = false;
    } else {
        notification_message(notif, &sequence_display_backlight_on);
        app->backlight_on = true;
    }
    notification_message(notif, &sequence_single_vibro);
    furi_delay_ms(50);
    notification_message(notif, &sequence_single_vibro);
    furi_record_close("notification");
}

void show_dialog(
    PomodoroApp* app,
    const char* title,
    const char* message,
    uint32_t timeout_ms,
    AlarmType alarm_type) {
    app->dialog_active = true;
    app->dialog_result = false;
    app->dialog_timeout = timeout_ms;
    app->dialog_start = furi_get_tick();
    strncpy(app->dialog_title, title, sizeof(app->dialog_title) - 1);
    strncpy(app->dialog_message, message, sizeof(app->dialog_message) - 1);
    app->current_alarm_type = alarm_type;
}
