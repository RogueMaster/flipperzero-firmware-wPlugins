#include "settings_menu.h"
#include "alert.h"

static const char* fmt_pattern(uint8_t v) {
    return alert_pattern_name((AlertPattern)(v % PatternCount));
}

static const char* fmt_tone(uint8_t v) {
    return alert_tone_name((AlertTone)(v % ToneCount));
}

static const char* fmt_tier(uint8_t v) {
    return alert_tier_name((ThreatTier)v);
}

static const char* fmt_mode(uint8_t v) {
    return v == ModeDecoy ? "Decoy" : "Sentinel";
}

static const char* fmt_bool(uint8_t v) {
    return v ? "On" : "Off";
}

uint8_t settings_menu_build(AlerterSettings* s, SettingRow* rows, uint8_t cap) {
    uint8_t n = 0;

#define ADD(lbl, fld, mx, f)                 \
    do {                                     \
        if(n < cap) {                        \
            rows[n].label = (lbl);           \
            rows[n].field = (uint8_t*)(fld); \
            rows[n].max = (mx);              \
            rows[n].fmt = (f);               \
            n++;                             \
        }                                    \
    } while(0)

    /* Mode is deliberately not exposed yet: Decoy is not wired into the
     * detection worker, so offering the choice would be a toggle that
     * silently does nothing. Re-add when the listener path lands (v0.3).
     *   ADD("Mode", &s->mode, ModeCount - 1, fmt_mode);
     */
    (void)fmt_mode;

    /* Vibration first: it is the primary channel for a pocket device. */
    ADD("Vibrate", &s->vibro_pattern, PatternCount - 1, fmt_pattern);
    ADD("Vibrate from", &s->min_tier_vibro, TierAlarm, fmt_tier);
    ADD("Force vibro", (uint8_t*)&s->force_vibro, 1, fmt_bool);

    ADD("Sound", &s->sound_pattern, PatternCount - 1, fmt_pattern);
    ADD("Sound from", &s->min_tier_sound, TierAlarm, fmt_tier);
    ADD("Tone", &s->tone, ToneCount - 1, fmt_tone);
    ADD("Volume", &s->volume, 10, NULL);
    ADD("Silent mode", (uint8_t*)&s->silent, 1, fmt_bool);

    ADD("LED", (uint8_t*)&s->led_enabled, 1, fmt_bool);
    ADD("Log to SD", (uint8_t*)&s->log_enabled, 1, fmt_bool);

#undef ADD
    return n;
}
