#include "notifications.h"

// NotificationSequence = const NotificationMessage*[]
// Each sequence is a NULL-terminated array of pointers.
//
// LED design:
//   Idle          = system default (no override, charging LED shows)
//   Permission    = magenta blink (persistent, needs user action)
//   Working       = blue solid
//   Dictation     = red blink
//   Turn complete = green solid
//   Interrupted   = yellow solid
//   Error         = red flash
//   Connected     = green solid
//   Disconnected  = red flash
//   Session end   = yellow flash
//   Ready         = cyan flash

// Restore: blue solid (applied after transient flash when LedStateWorking)
static const NotificationSequence seq_restore_working = {
    &message_blink_stop,
    &message_red_0,
    &message_green_0,
    &message_blue_255,
    &message_do_not_reset,
    NULL,
};

// Success: ascending C5-E5-G5 + vibro + green flash
static const NotificationSequence seq_success = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_0,
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    NULL,
};

// Error: low double beep G4-G4 + vibro + backlight + red flash
static const NotificationSequence seq_error = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_delay_50,
    &message_note_g4,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Alert: single E5 blip + cyan flash (no vibro)
static const NotificationSequence seq_alert = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_e5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Permission: rising C5-E5 + double vibro + backlight + magenta blink (persistent)
static const NotificationSequence seq_perm = {
    &message_display_backlight_on,
    &message_vibro_on,
    &message_note_c5,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_note_e5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_blink_start_10,
    &message_blink_set_color_magenta,
    &message_do_not_reset,
    NULL,
};

// Voice request: G5 blip + vibro + brief red flash (immediate button feedback)
static const NotificationSequence seq_voice_request = {
    &message_vibro_on,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Voice start LED-only: red blink (no sound, no vibro — used when button feedback was local)
static const NotificationSequence seq_voice_start_led = {
    &message_blink_start_10,
    &message_blink_set_color_red,
    &message_do_not_reset,
    NULL,
};

// Voice start: high ding (A5) + vibro + red blink (host confirmed)
static const NotificationSequence seq_voice_start = {
    &message_vibro_on,
    &message_note_a5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_blink_start_10,
    &message_blink_set_color_red,
    &message_do_not_reset,
    NULL,
};

// Voice stop: low dong (C5) + vibro + red flash then LED off
static const NotificationSequence seq_voice_stop = {
    &message_vibro_on,
    &message_blink_stop,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_c5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Voice stop quiet: LED reset only (no sound, no vibro)
static const NotificationSequence seq_voice_stop_quiet = {
    &message_blink_stop,
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// ESC: quick descending E5-C5 + yellow flash (no vibro)
static const NotificationSequence seq_esc = {
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_red_0,
    &message_green_0,
    NULL,
};

// Enter sent: short confirm blip + cyan flash (no vibro)
static const NotificationSequence seq_enter = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Command sent: C5 blip + vibro + cyan flash
static const NotificationSequence seq_cmd = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Connect: startup ascending C5-E5-G5-C6 + vibro + green solid
static const NotificationSequence seq_connect = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_0,
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_c6,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

// Disconnect: descending C6-G5-E5-C5 + vibro + red flash
static const NotificationSequence seq_disconnect = {
    &message_vibro_on,
    &message_red_255,
    &message_green_0,
    &message_blue_0,
    &message_note_c6,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    NULL,
};

// Interrupt: descending E5-C5 + vibro + yellow solid
static const NotificationSequence seq_interrupt = {
    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    &message_do_not_reset,
    NULL,
};

// Session end: descending C6-G5-E5-C5 + vibro + yellow flash
static const NotificationSequence seq_session_end = {
    &message_vibro_on,
    &message_red_255,
    &message_green_255,
    &message_blue_0,
    &message_note_c6,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_c5,
    &message_delay_250,
    &message_sound_off,
    &message_vibro_off,
    &message_red_0,
    &message_green_0,
    NULL,
};

// Ready: ascending C5-E5-G5 + vibro + cyan flash
static const NotificationSequence seq_ready = {
    &message_vibro_on,
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_100,
    &message_note_e5,
    &message_delay_100,
    &message_note_g5,
    &message_delay_100,
    &message_sound_off,
    &message_vibro_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// LED-only sequences (no sound)

// Working: blue solid
static const NotificationSequence seq_led_working = {
    &message_blink_stop,
    &message_red_0,
    &message_green_0,
    &message_blue_255,
    &message_do_not_reset,
    NULL,
};

// LED off: stop blinking and release back to system
static const NotificationSequence seq_led_off = {
    &message_blink_stop,
    &message_red_0,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Mute on: two descending notes (going quiet) + no LED change
static const NotificationSequence seq_mute_on = {
    &message_note_e5,
    &message_delay_50,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

// Mute off: two ascending notes (coming back) + no LED change
static const NotificationSequence seq_mute_off = {
    &message_note_c5,
    &message_delay_50,
    &message_note_e5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

// Compacting: cyan blink (persistent — context compaction in progress)
static const NotificationSequence seq_led_compact = {
    &message_blink_start_10,
    &message_blink_set_color_cyan,
    &message_do_not_reset,
    NULL,
};

// LED flash: brief cyan flash (no sound, no vibro)
static const NotificationSequence seq_led_flash = {
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_delay_50,
    &message_green_0,
    &message_blue_0,
    NULL,
};

// Compact done: stop blink + short C5 ding + cyan flash then off
static const NotificationSequence seq_compact_done = {
    &message_blink_stop,
    &message_red_0,
    &message_green_255,
    &message_blue_255,
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    &message_green_0,
    &message_blue_0,
    NULL,
};

static const NotificationSequence* const sound_table[] = {
    [SoundSuccess]        = &seq_success,
    [SoundError]          = &seq_error,
    [SoundAlert]          = &seq_alert,
    [SoundVoiceRequest]   = &seq_voice_request,
    [SoundVoiceStart]     = &seq_voice_start,
    [SoundVoiceStartLed]  = &seq_voice_start_led,
    [SoundVoiceStop]      = &seq_voice_stop,
    [SoundVoiceStopQuiet] = &seq_voice_stop_quiet,
    [SoundEsc]            = &seq_esc,
    [SoundEnter]          = &seq_enter,
    [SoundCmd]            = &seq_cmd,
    [SoundPerm]           = &seq_perm,
    [SoundConnect]        = &seq_connect,
    [SoundDisconnect]     = &seq_disconnect,
    [SoundLedWorking]     = &seq_led_working,
    [SoundLedOff]         = &seq_led_off,
    [SoundInterrupt]      = &seq_interrupt,
    [SoundSessionEnd]     = &seq_session_end,
    [SoundReady]          = &seq_ready,
    [SoundMuteOn]         = &seq_mute_on,
    [SoundMuteOff]        = &seq_mute_off,
    [SoundLedCompact]     = &seq_led_compact,
    [SoundCompactDone]    = &seq_compact_done,
    [SoundLedFlash]       = &seq_led_flash,
};

void notify_play(NotificationApp* app, SoundType sound, LedState restore) {
    if(!app) return;
    if((size_t)sound >= sizeof(sound_table) / sizeof(sound_table[0])) return;
    const NotificationSequence* seq = sound_table[sound];
    if(seq) notification_message(app, seq);
    if(restore == LedStateWorking) {
        notification_message(app, &seq_restore_working);
    }
}
