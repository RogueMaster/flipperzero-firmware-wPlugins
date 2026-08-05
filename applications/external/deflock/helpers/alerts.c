// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "alerts.h"

#include <notification/notification_messages.h>

// A short two-note rise (C5 -> G5) rather than the SDK's sequence_success or
// sequence_error: those already mean "report saved" / "report failed" elsewhere
// in the app, and a detection is neither. message_sound_off closes the tone so
// the speaker doesn't hold the last note.
#define ALERT_NOTES \
    &message_note_c5, &message_delay_50, &message_note_g5, &message_delay_50, &message_sound_off

static const NotificationSequence alert_seq_backlight = {
    &message_display_backlight_on,
    NULL,
};

static const NotificationSequence alert_seq_vibro = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};

static const NotificationSequence alert_seq_beep = {
    &message_display_backlight_on,
    ALERT_NOTES,
    NULL,
};

static const NotificationSequence alert_seq_both = {
    &message_display_backlight_on,
    &message_vibro_on,
    ALERT_NOTES,
    &message_vibro_off,
    NULL,
};

void recon_alert_fire(NotificationApp* notifications, uint8_t mode, bool sound_enabled) {
    if(!notifications) return;

    const NotificationSequence* seq = NULL;
    switch(mode) {
    case ReconAlertVibro:
        seq = &alert_seq_vibro;
        break;
    case ReconAlertBeep:
        // Sound off -> nothing audible is left of a beep-only alert, so fall back
        // to the backlight alone rather than silently doing nothing at all.
        // (if/else, not a ternary: each sequence is a differently-sized array, so
        // the two branches have incompatible pointer types.)
        if(sound_enabled) {
            seq = &alert_seq_beep;
        } else {
            seq = &alert_seq_backlight;
        }
        break;
    case ReconAlertBoth:
        if(sound_enabled) {
            seq = &alert_seq_both;
        } else {
            seq = &alert_seq_vibro;
        }
        break;
    default: // ReconAlertOff (or a corrupt setting value)
        return;
    }
    notification_message(notifications, seq);
}
