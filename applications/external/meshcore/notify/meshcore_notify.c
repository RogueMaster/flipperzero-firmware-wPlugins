#include "meshcore_notify.h"

#include <furi.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "MeshCoreNotify"

/* No status colour is held. Distinct from every MeshCoreSnrGrade value. */
#define MESHCORE_NOTIFY_NO_GRADE (-1)

struct MeshCoreNotify {
    NotificationApp* app;
    const MeshCoreSettings* settings;
    /* The status colour currently on the LED, or MESHCORE_NOTIFY_NO_GRADE.
     * Lets led_status skip re-sending a colour that is already lit. An event
     * blink resets the RGB layer, so meshcore_notify_event invalidates this
     * back to "none" and the next reading repaints the held colour. */
    int last_status_grade;
};

/* ---- custom sound-only sequences (the built-ins mix sound+led+vibro) ---- */

static const NotificationSequence seq_beep_ok = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_beep_fail = {
    &message_note_e5,
    &message_delay_50,
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_beep_short = {
    &message_note_c5,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

/* Incoming message — a rising c-e-g arpeggio, deliberately its own shape so a
 * message that lands mid-walk is told apart by ear from the ping beeps and the
 * single-tick mark. Kept short (~150 ms): it fires from the mailbox thread, and
 * a long sound would hold that thread on the notification queue. */
static const NotificationSequence seq_beep_incoming = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_note_g5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence seq_beep_alarm = {
    &message_note_c4,
    &message_delay_100,
    &message_note_c4,
    &message_delay_100,
    &message_note_c4,
    &message_delay_100,
    &message_sound_off,
    NULL,
};

/* Yellow held (there is a set_only for red/green/blue but not yellow). */
static const NotificationSequence seq_status_yellow = {
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_do_not_reset,
    NULL,
};

MeshCoreNotify* meshcore_notify_alloc(const MeshCoreSettings* settings) {
    MeshCoreNotify* notify = malloc(sizeof(MeshCoreNotify));
    notify->app = furi_record_open(RECORD_NOTIFICATION);
    notify->settings = settings;
    notify->last_status_grade = MESHCORE_NOTIFY_NO_GRADE;
    return notify;
}

void meshcore_notify_free(MeshCoreNotify* notify) {
    furi_assert(notify);
    /* Leave nothing lit behind us. */
    notification_message(notify->app, &sequence_reset_rgb);
    furi_record_close(RECORD_NOTIFICATION);
    free(notify);
}

/* Each aspect fires only if its toggle is on; the settings are read live.
 * They report back whether they actually emitted, which is the only way to
 * observe this module from outside: a beep, a buzz and a lit LED are all
 * invisible to the CLI, so the debug line below is what makes "the event
 * reached notify, and the toggle silenced it" a checkable claim rather than an
 * assertion. Debug level, so it costs nothing unless someone asks for it. */
static bool meshcore_notify_snd(MeshCoreNotify* n, const NotificationSequence* seq) {
    if(!n->settings->sound) return false;
    notification_message(n->app, seq);
    return true;
}
static bool meshcore_notify_vib(MeshCoreNotify* n, const NotificationSequence* seq) {
    if(!n->settings->vibro) return false;
    notification_message(n->app, seq);
    return true;
}
static bool meshcore_notify_led(MeshCoreNotify* n, const NotificationSequence* seq) {
    if(!n->settings->led) return false;
    notification_message(n->app, seq);
    return true;
}

static const char* meshcore_notify_name(MeshCoreNotifyEvent event) {
    switch(event) {
    case MeshCoreNotifyIncoming:
        return "incoming";
    case MeshCoreNotifyPingOk:
        return "ping-ok";
    case MeshCoreNotifyPingFail:
        return "ping-fail";
    case MeshCoreNotifyConnected:
        return "connected";
    case MeshCoreNotifyDisconnected:
        return "disconnected";
    case MeshCoreNotifyMark:
        return "mark";
    case MeshCoreNotifyApplyOk:
        return "apply-ok";
    case MeshCoreNotifyError:
        return "error";
    default:
        return "?";
    }
}

/* "fired" emitted, "off" wanted but the toggle is off, "-" not used here. */
static const char* meshcore_notify_state(bool used, bool fired) {
    if(!used) return "-";
    return fired ? "fired" : "off";
}

void meshcore_notify_event(MeshCoreNotify* notify, MeshCoreNotifyEvent event) {
    furi_assert(notify);

    bool use_snd = false, use_vib = false, use_led = false;
    bool did_snd = false, did_vib = false, did_led = false;

    switch(event) {
    case MeshCoreNotifyIncoming:
        /* Must not be missed: the strongest across all three — a double buzz, a
         * blue blink, and its own rising chime (not the mark's single tick). */
        use_vib = use_led = use_snd = true;
        did_vib = meshcore_notify_vib(notify, &sequence_double_vibro);
        did_led = meshcore_notify_led(notify, &sequence_blink_blue_100);
        did_snd = meshcore_notify_snd(notify, &seq_beep_incoming);
        break;
    case MeshCoreNotifyPingOk:
        /* Sound/vibro carry ping outcome on the move; no LED, so the logger's
         * held status colour is not disturbed. */
        use_snd = true;
        did_snd = meshcore_notify_snd(notify, &seq_beep_ok);
        break;
    case MeshCoreNotifyPingFail:
        use_snd = use_vib = true;
        did_snd = meshcore_notify_snd(notify, &seq_beep_fail);
        did_vib = meshcore_notify_vib(notify, &sequence_single_vibro);
        break;
    case MeshCoreNotifyConnected:
        use_led = use_vib = true;
        did_led = meshcore_notify_led(notify, &sequence_blink_green_100);
        did_vib = meshcore_notify_vib(notify, &sequence_single_vibro);
        break;
    case MeshCoreNotifyDisconnected:
        use_led = use_vib = true;
        did_led = meshcore_notify_led(notify, &sequence_blink_red_100);
        did_vib = meshcore_notify_vib(notify, &sequence_single_vibro);
        break;
    case MeshCoreNotifyMark:
        /* The tactile "your press counted" — vibro first, a tick of sound. */
        use_vib = use_snd = true;
        did_vib = meshcore_notify_vib(notify, &sequence_single_vibro);
        did_snd = meshcore_notify_snd(notify, &seq_beep_short);
        break;
    case MeshCoreNotifyApplyOk:
        use_snd = use_led = use_vib = true;
        did_snd = meshcore_notify_snd(notify, &seq_beep_ok);
        did_led = meshcore_notify_led(notify, &sequence_blink_green_100);
        did_vib = meshcore_notify_vib(notify, &sequence_single_vibro);
        break;
    case MeshCoreNotifyError:
        use_snd = use_led = use_vib = true;
        did_snd = meshcore_notify_snd(notify, &seq_beep_alarm);
        did_led = meshcore_notify_led(notify, &sequence_blink_red_100);
        did_vib = meshcore_notify_vib(notify, &sequence_double_vibro);
        break;
    default:
        break;
    }

    /* An LED blink just reset the RGB layer, wiping any held status colour.
     * Drop the cache so the logger's next reading repaints it rather than
     * skipping it as "already shown". */
    if(did_led) notify->last_status_grade = MESHCORE_NOTIFY_NO_GRADE;

    FURI_LOG_D(
        TAG,
        "%s sound:%s vibro:%s led:%s",
        meshcore_notify_name(event),
        meshcore_notify_state(use_snd, did_snd),
        meshcore_notify_state(use_vib, did_vib),
        meshcore_notify_state(use_led, did_led));
}

void meshcore_notify_led_status(MeshCoreNotify* notify, MeshCoreSnrGrade grade) {
    furi_assert(notify);
    if(!notify->settings->led) {
        /* Nothing is lit while the LED is off, so the cache must say so — else a
         * grade held before the toggle would block the repaint when it comes
         * back on at that same grade. Keeps "cache == what is lit" true no matter
         * how the toggle flipped, not only via the Settings scene's led_off. */
        notify->last_status_grade = MESHCORE_NOTIFY_NO_GRADE;
        return;
    }

    /* Already holding this colour: re-sending the same solid RGB is invisible,
     * so skip the redundant (queue-blocking) call. The logger calls this on
     * every packet, so this collapses a steady link to one LED write per grade
     * change. Invalidated by any event blink (above), and by led_off. */
    if((int)grade == notify->last_status_grade) return;
    notify->last_status_grade = (int)grade;

    switch(grade) {
    case MeshCoreSnrGood:
        FURI_LOG_D(TAG, "status led: green");
        notification_message(notify->app, &sequence_set_only_green_255);
        break;
    case MeshCoreSnrMarginal:
        FURI_LOG_D(TAG, "status led: yellow");
        notification_message(notify->app, &seq_status_yellow);
        break;
    case MeshCoreSnrBad:
    default:
        FURI_LOG_D(TAG, "status led: red");
        notification_message(notify->app, &sequence_set_only_red_255);
        break;
    }
}

void meshcore_notify_led_off(MeshCoreNotify* notify) {
    furi_assert(notify);
    notify->last_status_grade = MESHCORE_NOTIFY_NO_GRADE;
    notification_message(notify->app, &sequence_reset_rgb);
}

/* Easter egg — the Super Mario overworld theme (~7.5 s), long enough for a
 * little victory dance: the motor pulses on the beats and the RGB flashes a
 * colour per note. Volume forced so it is heard whatever the setting. The body
 * is machine-generated (tools/../gen_song.py) to keep 30 notes' worth of
 * interleaved sound+vibro+LED honest. */
static const NotificationSequence seq_song = {
    &message_force_speaker_volume_setting_1f,
    &message_red_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_blue_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_red_255,
    &message_note_c5,
    &message_delay_100,
    &message_delay_50,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_blue_255,
    &message_vibro_on,
    &message_note_g5,
    &message_delay_250,
    &message_delay_100,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_250,
    &message_red_255,
    &message_vibro_on,
    &message_note_g4,
    &message_delay_250,
    &message_delay_100,
    &message_vibro_off,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_250,
    &message_green_255,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_blue_255,
    &message_note_g4,
    &message_delay_100,
    &message_delay_50,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_red_255,
    &message_vibro_on,
    &message_note_e4,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_green_255,
    &message_note_a4,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_blue_255,
    &message_vibro_on,
    &message_note_b4,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_red_255,
    &message_note_as4,
    &message_delay_100,
    &message_delay_50,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_vibro_on,
    &message_note_a4,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_blue_255,
    &message_note_g4,
    &message_delay_100,
    &message_delay_50,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_red_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_note_g5,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_blue_255,
    &message_vibro_on,
    &message_note_a5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_red_255,
    &message_note_f5,
    &message_delay_100,
    &message_delay_50,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_vibro_on,
    &message_note_g5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_blue_255,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_red_255,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_note_d5,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_blue_255,
    &message_vibro_on,
    &message_note_b4,
    &message_delay_250,
    &message_delay_100,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_red_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_blue_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    &message_delay_100,
    &message_red_255,
    &message_note_c5,
    &message_delay_100,
    &message_delay_50,
    &message_red_0,
    &message_sound_off,
    &message_delay_25,
    &message_green_255,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_delay_50,
    &message_vibro_off,
    &message_green_0,
    &message_sound_off,
    &message_delay_25,
    &message_blue_255,
    &message_vibro_on,
    &message_note_g5,
    &message_delay_250,
    &message_delay_100,
    &message_vibro_off,
    &message_blue_0,
    &message_sound_off,
    &message_delay_25,
    &message_sound_off,
    NULL,
};

void meshcore_notify_play_song(MeshCoreNotify* notify) {
    furi_assert(notify);
    /* Block so the whole tune plays before the caller moves on. Bypasses the
     * settings gates by design — an explicit "party" request, not a field cue. */
    notification_message_block(notify->app, &seq_song);
    /* Leave nothing lit behind. */
    notify->last_status_grade = MESHCORE_NOTIFY_NO_GRADE;
    notification_message(notify->app, &sequence_reset_rgb);
}
