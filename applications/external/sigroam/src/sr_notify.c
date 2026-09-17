#include "sr_notify.h"
#include <notification/notification_messages.h> /* N9. N7: notes header is not in the Header table. */

/* N8: upstream values for c5 / g5, so notes.h (unregistered in the Header table) is not needed. */
static const NotificationMessage sr_msg_note_c5 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 523.25f, .volume = 1.0f},
};
static const NotificationMessage sr_msg_note_g5 = {
    .type = NotificationMessageTypeSoundOn,
    .data.sound = {.frequency = 783.99f, .volume = 1.0f},
};

/* N4: the stock success/error sequences mix sound+vibro+backlight+LED and cannot be
   gated per switch, so these three are built here. All are single-purpose. */
static const NotificationSequence sr_seq_vibro = {
    &message_vibro_on,
    &message_delay_100,
    &message_vibro_off,
    NULL,
};
static const NotificationSequence sr_seq_beep_lost = {
    /* falling: g5 -> c5 */
    &sr_msg_note_g5,
    &message_delay_100,
    &sr_msg_note_c5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};
static const NotificationSequence sr_seq_beep_acquired = {
    /* rising: c5 -> g5 */
    &sr_msg_note_c5,
    &message_delay_100,
    &sr_msg_note_g5,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

void sr_notify_alert(NotificationApp* n, SrAlertKind kind, const SrSettings* s) {
    if(n == NULL || kind == SrAlertNone || s == NULL) {
        return;
    }

    /* Order is fixed: vibro, then beep, then LED. */
    if(sr_settings_effective_vibro(s)) {
        notification_message(n, &sr_seq_vibro);
    }
    if(sr_settings_effective_sound(s)) {
        if(kind == SrAlertGpsFixLost) {
            notification_message(n, &sr_seq_beep_lost);
        } else if(kind == SrAlertGpsFixAcquired) {
            notification_message(n, &sr_seq_beep_acquired);
        }
    }
    if(!s->stealth) {
        if(kind == SrAlertGpsFixLost) {
            notification_message(n, &sequence_blink_red_100);
        } else if(kind == SrAlertGpsFixAcquired) {
            notification_message(n, &sequence_blink_green_100);
        }
    }
}

void sr_notify_backlight_enforce(NotificationApp* n, bool on) {
    if(n == NULL) {
        return;
    }
    notification_message(
        n, on ? &sequence_display_backlight_enforce_on : &sequence_display_backlight_enforce_auto);
}
