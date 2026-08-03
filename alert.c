/*
 * Alert engine.
 *
 * Vibration is the primary channel and sound is secondary: this device lives
 * in a pocket, where a beep is muffled by fabric and useless in a loud room.
 *
 * The vibro motor is binary (furi_hal_vibro_on takes a bool -- no PWM), so
 * patterns are expressed purely in the time domain. Sound uses the same
 * pattern vocabulary so the two stay conceptually aligned.
 */

#include "alert.h"

#include <furi_hal_speaker.h>
#include <notification/notification_messages.h>

/* A pattern is a list of on/off step durations in ms, replayed by a worker
 * thread. Ending with a 0 marks the loop point. */
typedef struct {
    const uint16_t* steps; /* alternating on,off,on,off... durations */
    uint8_t len;
    bool loop;
} PatternDef;

static const uint16_t steps_single[] = {150, 0};
static const uint16_t steps_double[] = {120, 100, 120, 0};
static const uint16_t steps_sos[] = {
    100, 80, 100, 80, 100, 250, /* S: dot dot dot */
    280, 80, 280, 80, 280, 250, /* O: dash dash dash */
    100, 80, 100, 80, 100, 600, /* S */
};
static const uint16_t steps_pulse[] = {200, 400};
static const uint16_t steps_continuous[] = {1000, 0};

static const PatternDef patterns[PatternCount] = {
    [PatternOff] = {NULL, 0, false},
    [PatternSingle] = {steps_single, 2, false},
    [PatternDouble] = {steps_double, 4, false},
    [PatternSos] = {steps_sos, 18, true},
    [PatternPulse] = {steps_pulse, 2, true},
    [PatternContinuous] = {steps_continuous, 2, true},
};

static const float tone_freq[ToneCount] = {1000.0f, 2000.0f, 3000.0f, 4000.0f};

struct Alert {
    NotificationApp* notifications;
    AlerterSettings* settings;

    FuriThread* thread;
    FuriMutex* mutex;
    volatile bool running;
    volatile ThreatTier tier; /* 0 = idle */

    bool speaker_held;
    bool vibro_on;
};

/* ---------- hardware helpers ---------- */

static void speaker_on(Alert* a) {
    if(a->speaker_held) return;
    if(furi_hal_speaker_acquire(SPEAKER_TIMEOUT)) {
        float vol = (float)a->settings->volume / 10.0f;
        furi_hal_speaker_start(tone_freq[a->settings->tone % ToneCount], vol);
        a->speaker_held = true;
    } else {
        /* Arbitrated resource -- another app may hold it. Degrade to
         * vibro+LED rather than failing the whole alert. */
        FURI_LOG_W(TAG, "speaker unavailable");
    }
}

static void speaker_off(Alert* a) {
    if(!a->speaker_held) return;
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    a->speaker_held = false;
}

static void vibro_set(Alert* a, bool on) {
    if(a->vibro_on == on) return;
    a->vibro_on = on;
    notification_message(
        a->notifications, on ? &sequence_set_vibro_on : &sequence_reset_vibro);
}

static void all_off(Alert* a) {
    speaker_off(a);
    vibro_set(a, false);
    if(a->settings->led_enabled) {
        notification_message(a->notifications, &sequence_reset_rgb);
    }
}

/* ---------- pattern playback ---------- */

static bool tier_enabled(uint8_t min_tier, ThreatTier tier) {
    return tier != TierNone && (uint8_t)tier >= min_tier;
}

static int32_t alert_thread(void* ctx) {
    Alert* a = ctx;

    while(a->running) {
        ThreatTier tier = a->tier;

        if(tier == TierNone) {
            all_off(a);
            furi_delay_ms(50);
            continue;
        }

        const AlerterSettings* s = a->settings;
        bool want_vibro =
            s->vibro_pattern != PatternOff && tier_enabled(s->min_tier_vibro, tier);
        bool want_sound = !s->silent && s->sound_pattern != PatternOff &&
                          tier_enabled(s->min_tier_sound, tier);

        if(s->led_enabled) {
            notification_message(
                a->notifications,
                tier == TierAlarm ? &sequence_set_only_red_255 :
                                    &sequence_set_only_blue_255);
        }

        /* Vibro drives the timing when active -- it is the primary channel.
         * Sound follows the same steps so the two stay in phase. */
        const PatternDef* vp = &patterns[s->vibro_pattern % PatternCount];
        const PatternDef* sp = &patterns[s->sound_pattern % PatternCount];
        const PatternDef* lead = want_vibro ? vp : (want_sound ? sp : NULL);

        if(!lead || lead->len == 0) {
            /* Nothing audible or tactile for this tier -- LED only. */
            furi_delay_ms(100);
            continue;
        }

        for(uint8_t i = 0; i < lead->len && a->running && a->tier == tier; i++) {
            bool on_phase = (i % 2) == 0;
            uint16_t dur = lead->steps[i];
            if(dur == 0) continue;

            if(on_phase) {
                if(want_vibro) vibro_set(a, true);
                if(want_sound) speaker_on(a);
            } else {
                vibro_set(a, false);
                speaker_off(a);
            }

            /* Sleep in slices so tier changes and exit are responsive. */
            uint16_t left = dur;
            while(left && a->running && a->tier == tier) {
                uint16_t slice = left > 20 ? 20 : left;
                furi_delay_ms(slice);
                left -= slice;
            }
        }

        vibro_set(a, false);
        speaker_off(a);

        if(!lead->loop) {
            /* One-shot: hold quiet until the tier changes. */
            while(a->running && a->tier == tier) {
                furi_delay_ms(50);
            }
        }
    }

    all_off(a);
    return 0;
}

/* ---------- public ---------- */

Alert* alert_alloc(NotificationApp* notifications, AlerterSettings* settings) {
    Alert* a = malloc(sizeof(Alert));
    a->notifications = notifications;
    a->settings = settings;
    a->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    a->running = true;
    a->tier = TierNone;
    a->speaker_held = false;
    a->vibro_on = false;

    a->thread = furi_thread_alloc_ex("NfcAlerterAlert", 1024, alert_thread, a);
    furi_thread_start(a->thread);
    return a;
}

void alert_free(Alert* a) {
    a->running = false;
    a->tier = TierNone;
    furi_thread_join(a->thread);
    furi_thread_free(a->thread);
    all_off(a);
    furi_mutex_free(a->mutex);
    free(a);
}

void alert_start(Alert* a, ThreatTier tier) {
    if(tier > a->tier) a->tier = tier;
}

void alert_stop(Alert* a) {
    a->tier = TierNone;
}

void alert_test(Alert* a) {
    ThreatTier saved = a->tier;
    /* Audition at Alarm so the user hears the loudest configured behavior. */
    a->tier = TierAlarm;
    furi_delay_ms(900);
    a->tier = saved;
}

const char* alert_pattern_name(AlertPattern p) {
    switch(p) {
    case PatternOff: return "Off";
    case PatternSingle: return "Single";
    case PatternDouble: return "Double";
    case PatternSos: return "SOS";
    case PatternPulse: return "Pulse";
    case PatternContinuous: return "Constant";
    default: return "?";
    }
}

const char* alert_tone_name(AlertTone t) {
    switch(t) {
    case ToneLow: return "1kHz";
    case ToneMid: return "2kHz";
    case ToneHigh: return "3kHz";
    case ToneVeryHigh: return "4kHz";
    default: return "?";
    }
}

const char* alert_tier_name(ThreatTier t) {
    switch(t) {
    case TierNone: return "None";
    case TierInfo: return "Info";
    case TierWarn: return "Warn";
    case TierAlarm: return "ALARM";
    default: return "?";
    }
}
