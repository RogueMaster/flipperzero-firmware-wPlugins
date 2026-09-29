#include "settings.h"

#include <lib/toolbox/saved_struct.h>
#include <applications/services/storage/storage.h>

#define SETTINGS_PATH  INT_PATH(".ghosttag.settings")
#define SETTINGS_MAGIC 0x47u /* 'G' */
#define SETTINGS_VER   1u

static const char* const sens_labels[GHOSTTAG_SENS_COUNT] = {"Near", "Medium", "Far"};
static const int8_t sens_cutoffs[GHOSTTAG_SENS_COUNT] = {-60, -75, -92};
static const char* const sens_hints[GHOSTTAG_SENS_COUNT] = {"~5 m", "~15 m", "all"};

static const char* const follow_labels[GHOSTTAG_FOLLOW_COUNT] =
    {"1 min", "3 min", "5 min", "10 min"};
static const uint32_t follow_minutes[GHOSTTAG_FOLLOW_COUNT] = {1, 3, 5, 10};

void ghosttag_settings_defaults(GhostTagSettings* s) {
    furi_assert(s);
    s->sensitivity_index = 1; /* Medium */
    s->follow_index = 1; /* 3 min */
    s->sound = true;
    s->vibro = true;
    s->led = true;
    s->keep_lit = true;
    s->log_session = false;
}

void ghosttag_settings_sanitise(GhostTagSettings* s) {
    furi_assert(s);
    if(s->sensitivity_index >= GHOSTTAG_SENS_COUNT) s->sensitivity_index = 1;
    if(s->follow_index >= GHOSTTAG_FOLLOW_COUNT) s->follow_index = 1;
    /* A _Bool that is neither 0 nor 1 is UB as soon as it is read. */
    s->sound = !!s->sound;
    s->vibro = !!s->vibro;
    s->led = !!s->led;
    s->keep_lit = !!s->keep_lit;
    s->log_session = !!s->log_session;
}

bool ghosttag_settings_load(GhostTagSettings* out) {
    furi_assert(out);
    bool ok = saved_struct_load(
        SETTINGS_PATH, out, sizeof(GhostTagSettings), SETTINGS_MAGIC, SETTINGS_VER);
    if(!ok) {
        ghosttag_settings_defaults(out);
        return false;
    }
    ghosttag_settings_sanitise(out);
    return true;
}

bool ghosttag_settings_save(const GhostTagSettings* in) {
    furi_assert(in);
    return saved_struct_save(
        SETTINGS_PATH, (void*)in, sizeof(GhostTagSettings), SETTINGS_MAGIC, SETTINGS_VER);
}

int8_t ghosttag_settings_rssi_cutoff(const GhostTagSettings* s) {
    return sens_cutoffs[s->sensitivity_index % GHOSTTAG_SENS_COUNT];
}

uint32_t ghosttag_settings_follow_ms(const GhostTagSettings* s) {
    return follow_minutes[s->follow_index % GHOSTTAG_FOLLOW_COUNT] * 60UL * 1000UL;
}

const char* ghosttag_settings_sensitivity_label(uint8_t index) {
    return sens_labels[index % GHOSTTAG_SENS_COUNT];
}

const char* ghosttag_settings_follow_label(uint8_t index) {
    return follow_labels[index % GHOSTTAG_FOLLOW_COUNT];
}

const char* ghosttag_settings_range_hint(uint8_t index) {
    return sens_hints[index % GHOSTTAG_SENS_COUNT];
}
