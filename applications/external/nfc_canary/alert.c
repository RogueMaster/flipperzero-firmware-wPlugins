/*
 * Alert engine.
 *
 * Vibration is the primary channel and sound is secondary: this device lives
 * in a pocket, where a beep is muffled by fabric and useless in a loud room.
 *
 * The vibro motor is binary (furi_hal_vibro_on takes a bool -- no PWM), so
 * patterns are expressed purely in the time domain. Sound uses the same
 * pattern vocabulary, but the two channels play INDEPENDENTLY: each advances
 * through its own selected pattern on a shared 10ms tick.
 */

#include "alert.h"

#include <furi_hal_speaker.h>
#include <notification/notification_messages.h>

/* A pattern is a list of on/off step durations in ms, replayed by a worker
 * thread. `loop` restarts from step 0 instead of stopping. */
typedef struct {
    const uint16_t* steps; /* alternating on,off,on,off... durations */
    uint8_t len;
    bool loop;
} PatternDef;

/* Steps alternate on,off,on,off... starting with ON. Every entry is a real
 * step -- there is no terminator value, so `len` must be exact. One-shot
 * patterns end on an ON step; the player turns the channel off when it runs
 * past the end. */
static const uint16_t steps_single[] = {150};
static const uint16_t steps_double[] = {120, 100, 120};
static const uint16_t steps_sos[] = {
    100,
    80,
    100,
    80,
    100,
    250, /* S: dot dot dot */
    280,
    80,
    280,
    80,
    280,
    250, /* O: dash dash dash */
    100,
    80,
    100,
    80,
    100,
    600, /* S, then a gap before repeating */
};
static const uint16_t steps_pulse[] = {200, 400};
/* Continuous is a long ON that loops, so it reads as unbroken. */
static const uint16_t steps_continuous[] = {60000};

#define ARRLEN(x) (uint8_t)(sizeof(x) / sizeof((x)[0]))

static const PatternDef patterns[PatternCount] = {
    [PatternOff] = {NULL, 0, false},
    [PatternSingle] = {steps_single, ARRLEN(steps_single), false},
    [PatternDouble] = {steps_double, ARRLEN(steps_double), false},
    [PatternSos] = {steps_sos, ARRLEN(steps_sos), true},
    [PatternPulse] = {steps_pulse, ARRLEN(steps_pulse), true},
    [PatternContinuous] = {steps_continuous, ARRLEN(steps_continuous), true},
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
    volatile uint32_t test_until; /* tick to auto-stop an audition; 0 = not testing */
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
    notification_message(a->notifications, on ? &sequence_set_vibro_on : &sequence_reset_vibro);
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

/* True while playback for `tier` should continue. Checked inside the playback
 * loops too, so a looping pattern (SOS/Pulse/Constant) still honors the
 * audition timeout instead of running forever. */
static bool alert_active(Alert* a, ThreatTier tier) {
    if(!a->running || a->tier != tier) return false;
    if(a->test_until && furi_get_tick() >= a->test_until) return false;
    return true;
}

static int32_t alert_thread(void* ctx) {
    Alert* a = ctx;

    while(a->running) {
        /* An audition ends on its own so the UI stays responsive during it. */
        if(a->test_until && furi_get_tick() >= a->test_until) {
            a->test_until = 0;
            a->tier = TierNone;
        }

        ThreatTier tier = a->tier;

        if(tier == TierNone) {
            all_off(a);
            furi_delay_ms(50);
            continue;
        }

        const AlerterSettings* s = a->settings;
        bool want_vibro = s->vibro_pattern != PatternOff && tier_enabled(s->min_tier_vibro, tier);
        bool want_sound = !s->silent && s->sound_pattern != PatternOff &&
                          tier_enabled(s->min_tier_sound, tier);

        if(s->led_enabled) {
            notification_message(
                a->notifications,
                tier == TierAlarm ? &sequence_set_only_red_255 : &sequence_set_only_blue_255);
        }

        /* Vibro and sound each run their OWN pattern. They are independent
         * channels with independent settings, so a single shared timeline
         * would silently ignore one of the two selections. Advance both on a
         * common tick and let each hit its own step boundaries. */
        const PatternDef* vp = want_vibro ? &patterns[s->vibro_pattern % PatternCount] : NULL;
        const PatternDef* sp = want_sound ? &patterns[s->sound_pattern % PatternCount] : NULL;

        if((!vp || vp->len == 0) && (!sp || sp->len == 0)) {
            /* Nothing audible or tactile for this tier -- LED only. */
            furi_delay_ms(100);
            continue;
        }

        uint8_t vi = 0, si = 0; /* current step index per channel */
        uint16_t v_left = vp && vp->len ? vp->steps[0] : 0;
        uint16_t s_left = sp && sp->len ? sp->steps[0] : 0;
        bool v_done = !(vp && vp->len);
        bool s_done = !(sp && sp->len);

        /* Prime the first on-phase of each channel. */
        if(!v_done) vibro_set(a, true);
        if(!s_done) speaker_on(a);

        const uint16_t TICK = 10;

        while(alert_active(a, tier) && !(v_done && s_done)) {
            furi_delay_ms(TICK);

            if(!v_done) {
                if(v_left > TICK) {
                    v_left -= TICK;
                } else {
                    vi++;
                    if(vi >= vp->len) {
                        if(vp->loop) {
                            vi = 0;
                        } else {
                            vibro_set(a, false);
                            v_done = true;
                        }
                    }
                    if(!v_done) {
                        v_left = vp->steps[vi];
                        vibro_set(a, (vi % 2) == 0);
                        /* A zero-length step is a no-op placeholder. */
                        if(v_left == 0) v_left = 1;
                    }
                }
            }

            if(!s_done) {
                if(s_left > TICK) {
                    s_left -= TICK;
                } else {
                    si++;
                    if(si >= sp->len) {
                        if(sp->loop) {
                            si = 0;
                        } else {
                            speaker_off(a);
                            s_done = true;
                        }
                    }
                    if(!s_done) {
                        s_left = sp->steps[si];
                        if((si % 2) == 0) {
                            speaker_on(a);
                        } else {
                            speaker_off(a);
                        }
                        if(s_left == 0) s_left = 1;
                    }
                }
            }
        }

        vibro_set(a, false);
        speaker_off(a);

        /* Both channels finished a one-shot: stay quiet until tier changes. */
        if(v_done && s_done) {
            while(alert_active(a, tier)) {
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

    a->thread = furi_thread_alloc_ex("NfcCanaryAlert", 1024, alert_thread, a);
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

/* Auto-cancel window for the audition. Long enough for a full SOS cycle
 * (~2.4s); a shorter test would cut the longer patterns off mid-way and make
 * them indistinguishable from each other. */
#define TEST_DURATION_MS 2600

void alert_test(Alert* a) {
    /* Non-blocking: this is called from the input thread, so sleeping here
     * would freeze the UI and queue up keypresses. The alert thread plays the
     * pattern; test_until tells it when to stop. */
    a->test_until = furi_get_tick() + furi_ms_to_ticks(TEST_DURATION_MS);
    alert_start(a, TierAlarm);
}

const char* alert_pattern_name(AlertPattern p) {
    switch(p) {
    case PatternOff:
        return "Off";
    case PatternSingle:
        return "Single";
    case PatternDouble:
        return "Double";
    case PatternSos:
        return "SOS";
    case PatternPulse:
        return "Pulse";
    case PatternContinuous:
        return "Constant";
    default:
        return "?";
    }
}

const char* alert_tone_name(AlertTone t) {
    switch(t) {
    case ToneLow:
        return "1kHz";
    case ToneMid:
        return "2kHz";
    case ToneHigh:
        return "3kHz";
    case ToneVeryHigh:
        return "4kHz";
    default:
        return "?";
    }
}

const char* alert_tier_name(ThreatTier t) {
    switch(t) {
    case TierNone:
        return "None";
    case TierInfo:
        return "Info";
    case TierWarn:
        return "Warn";
    case TierAlarm:
        return "ALARM";
    default:
        return "?";
    }
}
