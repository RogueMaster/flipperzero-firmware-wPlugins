#include "signals.h"

// Red LED blink building block (one flash).
#define LED_BLINK \
    &message_red_255, &message_delay_100, &message_red_0, &message_delay_100

// LED only: three red flashes.
static const NotificationSequence seq_led = {
    LED_BLINK, LED_BLINK, LED_BLINK, NULL,
};

// Vibro: warning = one pulse, critical = three pulses.
static const NotificationSequence seq_vibro_warn = {
    &message_red_255, &message_vibro_on, &message_delay_250, &message_vibro_off,
    &message_red_0, NULL,
};

static const NotificationSequence seq_vibro_crit = {
    &message_red_255, &message_vibro_on, &message_delay_100, &message_vibro_off,
    &message_delay_100, &message_vibro_on, &message_delay_100, &message_vibro_off,
    &message_delay_100, &message_vibro_on, &message_delay_250, &message_vibro_off,
    &message_red_0, NULL,
};

// Sound: warning = rising two-tone, critical = alternating siren-like sequence.
static const NotificationSequence seq_sound_warn = {
    &message_red_255, &message_note_c5, &message_delay_100, &message_note_g5,
    &message_delay_250, &message_sound_off, &message_red_0, NULL,
};

static const NotificationSequence seq_sound_crit = {
    &message_red_255, &message_note_e6, &message_delay_100, &message_note_a5,
    &message_delay_100, &message_note_e6, &message_delay_100, &message_note_a5,
    &message_delay_100, &message_note_e6, &message_delay_250, &message_sound_off,
    &message_red_0, NULL,
};

static const NotificationSequence seq_both_warn = {
    &message_red_255, &message_vibro_on, &message_note_c5, &message_delay_100,
    &message_note_g5, &message_delay_250, &message_sound_off, &message_vibro_off,
    &message_red_0, NULL,
};

static const NotificationSequence seq_both_crit = {
    &message_red_255, &message_vibro_on, &message_note_e6, &message_delay_100,
    &message_note_a5, &message_delay_100, &message_note_e6, &message_delay_100,
    &message_note_a5, &message_delay_100, &message_note_e6, &message_delay_250,
    &message_sound_off, &message_vibro_off, &message_red_0, NULL,
};

void phr_signal_play(NotificationApp* n, AlertSignal signal, bool critical) {
    // (sequences have different lengths, so their pointer types differ: no ?: here)
    switch(signal) {
    case AlertSignalVibro:
        if(critical)
            notification_message(n, &seq_vibro_crit);
        else
            notification_message(n, &seq_vibro_warn);
        break;
    case AlertSignalSound:
        if(critical)
            notification_message(n, &seq_sound_crit);
        else
            notification_message(n, &seq_sound_warn);
        break;
    case AlertSignalVibroSound:
        if(critical)
            notification_message(n, &seq_both_crit);
        else
            notification_message(n, &seq_both_warn);
        break;
    case AlertSignalLed:
        notification_message(n, &seq_led);
        break;
    default:
        break;
    }
}

bool phr_signal_is_critical(AlertRuleId rule) {
    return rule == AlertCpuTemp || rule == AlertGpuTemp || rule == AlertBatteryLow ||
           rule == AlertLinkLost;
}
