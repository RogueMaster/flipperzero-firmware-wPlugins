#include "specter_settings.h"

#include "field_scale.h"

#include <furi.h>
#include <saved_struct.h>
#include <storage/storage.h>

#define SETTINGS_PATH    APP_DATA_PATH("specter.conf")
#define SETTINGS_MAGIC   0x5Cu
/* Bumped in 2.3 when the Meter setting was added. saved_struct validates size
 * as well as version, so an older file is simply ignored and the defaults come
 * back - a one-time reset of preferences rather than a garbled struct. */
#define SETTINGS_VERSION 2u

static const char* const sens_labels[SPECTER_SENS_COUNT] = {"High", "Medium", "Low", "Custom"};
static const uint8_t sens_thresh[SPECTER_SENS_COUNT] = {0, 8, 20, 0}; // Custom uses its own

static const char* const survey_labels[SPECTER_SURVEY_COUNT] = {"30 s", "60 s", "2 min"};
static const uint32_t survey_seconds[SPECTER_SURVEY_COUNT] = {30, 60, 120};

void specter_settings_set_defaults(SpecterSettings* s) {
    furi_assert(s);
    s->sensitivity_index = 1; // Medium
    s->custom_threshold = 5;
    s->survey_index = 1; // 60 s
    s->sound = true;
    s->vibro = true;
    s->led = true;
    s->stealth = false;
    s->logging = true;
    s->meter_raw = false; // full-scale meter by default; see field_scale.h
}

uint8_t specter_settings_full_scale(const SpecterSettings* s) {
    furi_assert(s);
    return s->meter_raw ? SPECTER_SCALE_RAW : SPECTER_FULL_SCALE_DUTY;
}

/* Anything read off the SD card is untrusted input as far as the label tables
 * are concerned. Clamp before it can be used as an index. */
static void specter_settings_sanitise(SpecterSettings* s) {
    if(s->sensitivity_index >= SPECTER_SENS_COUNT) s->sensitivity_index = 1;
    if(s->survey_index >= SPECTER_SURVEY_COUNT) s->survey_index = 1;
    if(s->custom_threshold > 90) s->custom_threshold = 90;
}

void specter_settings_load(SpecterSettings* s) {
    furi_assert(s);
    specter_settings_set_defaults(s);

    SpecterSettings loaded;
    if(saved_struct_load(
           SETTINGS_PATH, &loaded, sizeof(loaded), SETTINGS_MAGIC, SETTINGS_VERSION)) {
        specter_settings_sanitise(&loaded);
        *s = loaded;
    }
}

bool specter_settings_save(const SpecterSettings* s) {
    furi_assert(s);

    /* The app data directory does not exist until something creates it. */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);

    return saved_struct_save(
        SETTINGS_PATH, s, sizeof(SpecterSettings), SETTINGS_MAGIC, SETTINGS_VERSION);
}

uint8_t specter_settings_threshold(const SpecterSettings* s) {
    furi_assert(s);
    uint8_t i = s->sensitivity_index % SPECTER_SENS_COUNT;
    if(i == SPECTER_SENS_CUSTOM) return s->custom_threshold;
    return sens_thresh[i];
}

const char* specter_settings_sensitivity_label(uint8_t index) {
    return sens_labels[index % SPECTER_SENS_COUNT];
}

const char* specter_settings_survey_label(uint8_t index) {
    return survey_labels[index % SPECTER_SURVEY_COUNT];
}

uint32_t specter_settings_survey_seconds(const SpecterSettings* s) {
    furi_assert(s);
    return survey_seconds[s->survey_index % SPECTER_SURVEY_COUNT];
}
