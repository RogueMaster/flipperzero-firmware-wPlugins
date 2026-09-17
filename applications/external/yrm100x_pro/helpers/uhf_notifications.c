#include "uhf_notifications.h"

static const NotificationSequence uhf_sequence_epc_sound_only = {
    &message_note_c6,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence uhf_sequence_success_sound_only = {
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence uhf_sequence_error_sound_only = {
    &message_note_c4,
    &message_delay_250,
    &message_sound_off,
    NULL,
};

static const NotificationSequence uhf_sequence_epc_vibro_only = {
    &message_vibro_on,
    &message_delay_25,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence uhf_sequence_success_vibro_only = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence uhf_sequence_error_vibro_only = {
    &message_vibro_on,
    &message_delay_250,
    &message_vibro_off,
    NULL,
};

static void uhf_notify_dispatch(
    UHFReaderApp* App,
    const NotificationSequence* both,
    const NotificationSequence* sound_only,
    const NotificationSequence* vibro_only) {
    if(!App || !App->Notifications) return;

    bool sound = App->SettingSoundIndex != 0U;
    bool vibro = App->SettingVibrationIndex != 0U;

    if(sound && vibro) {
        notification_message(App->Notifications, both);
    } else if(sound) {
        notification_message(App->Notifications, sound_only);
    } else if(vibro) {
        notification_message(App->Notifications, vibro_only);
    }
}

void uhf_notify_epc(UHFReaderApp* App) {
    uhf_notify_dispatch(
        App, &sequence_semi_success, &uhf_sequence_epc_sound_only, &uhf_sequence_epc_vibro_only);
}

void uhf_notify_success(UHFReaderApp* App) {
    uhf_notify_dispatch(
        App, &sequence_success, &uhf_sequence_success_sound_only, &uhf_sequence_success_vibro_only);
}

void uhf_notify_error(UHFReaderApp* App) {
    uhf_notify_dispatch(
        App, &sequence_error, &uhf_sequence_error_sound_only, &uhf_sequence_error_vibro_only);
}
