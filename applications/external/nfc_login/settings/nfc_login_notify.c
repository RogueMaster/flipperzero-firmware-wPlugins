#include "nfc_login_notify.h"
#include <notification/notification_messages.h>

static const NotificationSequence sequence_success_vibro = {
    &message_display_backlight_on,
    &message_green_255,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_error_vibro = {
    &message_display_backlight_on,
    &message_red_255,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_100,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence sequence_success_sound = {
    &message_display_backlight_on,
    &message_green_255,
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_error_sound = {
    &message_display_backlight_on,
    &message_red_255,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    &message_delay_100,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_success_visual = {
    &message_display_backlight_on,
    &message_green_255,
    NULL,
};

static const NotificationSequence sequence_error_visual = {
    &message_display_backlight_on,
    &message_red_255,
    NULL,
};

void nfc_login_notify(App* app, NfcLoginNotifyType type) {
    if(!app || !app->notification) {
        return;
    }

    const bool success = (type == NfcLoginNotifySuccess);

    if(app->sound_enabled && app->vibro_enabled) {
        if(success) {
            notification_message(app->notification, &sequence_success);
        } else {
            notification_message(app->notification, &sequence_error);
        }
    } else if(app->sound_enabled) {
        if(success) {
            notification_message(app->notification, &sequence_success_sound);
        } else {
            notification_message(app->notification, &sequence_error_sound);
        }
    } else if(app->vibro_enabled) {
        if(success) {
            notification_message(app->notification, &sequence_success_vibro);
        } else {
            notification_message(app->notification, &sequence_error_vibro);
        }
    } else if(success) {
        notification_message(app->notification, &sequence_success_visual);
    } else {
        notification_message(app->notification, &sequence_error_visual);
    }
}
