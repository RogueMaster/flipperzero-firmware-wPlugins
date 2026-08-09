#include "settings.h"

#include <toolbox/saved_struct.h>

void settings_defaults(AlerterSettings* s) {
    s->mode = ModeSentinel; /* passive by default -- see README on Decoy */
    s->vibro_pattern = PatternDouble;
    s->sound_pattern = PatternPulse;
    s->tone = ToneMid; /* 2kHz: piezo is loudest 2-4kHz, cuts through fabric */
    s->volume = 10;
    /* Vibration is the primary channel, so it engages one tier earlier than
     * sound. A pocket alert should be felt before it is heard. */
    s->min_tier_vibro = TierWarn;
    s->min_tier_sound = TierAlarm;
    s->silent = false;
    s->force_vibro = true; /* an alarm should not be muted by a global setting */
    s->led_enabled = true;
    s->log_enabled = true;
    s->seen_intro = false;
}

void settings_load(AlerterSettings* s) {
    settings_defaults(s);
    AlerterSettings loaded;
    if(saved_struct_load(
           SETTINGS_PATH, &loaded, sizeof(AlerterSettings), SETTINGS_MAGIC, SETTINGS_VER)) {
        *s = loaded;
    }
}

void settings_save(const AlerterSettings* s) {
    saved_struct_save(
        SETTINGS_PATH, (void*)s, sizeof(AlerterSettings), SETTINGS_MAGIC, SETTINGS_VER);
}
