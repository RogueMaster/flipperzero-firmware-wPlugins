#include "specter_settings.h"

#include "field_scale.h"

#include <furi.h>
#include <saved_struct.h>
#include <storage/storage.h>

#define SETTINGS_PATH       APP_DATA_PATH("specter.conf")
#define SETTINGS_MAGIC      0x5Cu
/* 2 in 2.3 when the Meter setting was added; 3 in 3.1 for the intro switch.
 *
 * saved_struct validates size as well as version, so adding a field makes every
 * previously saved file unloadable. Up to now that meant a silent one-time
 * reset of everyone's preferences - sensitivity, survey length, stealth, the
 * lot - as the price of one new checkbox. That is a bad trade, and it is
 * avoidable: the old layout is the exact prefix of the new one, so the previous
 * version is tried as a fallback and copied forward. */
#define SETTINGS_VERSION    3u
#define SETTINGS_VERSION_V2 2u

/* The v2 layout, frozen. Do not edit this to match SpecterSettings - the whole
 * point is that it describes what is already on people's SD cards. */
typedef struct {
    uint8_t sensitivity_index;
    uint8_t custom_threshold;
    uint8_t survey_index;
    bool sound;
    bool vibro;
    bool led;
    bool stealth;
    bool logging;
    bool meter_raw;
} SpecterSettingsV2;

static const char* const sens_labels[SPECTER_SENS_COUNT] = {"High", "Medium", "Low", "Custom"};
static const uint8_t sens_thresh[SPECTER_SENS_COUNT] = {0, 8, 20, 0}; // Custom uses its own

/* The meter-scale vocabulary, in ONE place.
 *
 * These words were previously duplicated: the Settings list said "0-100" and
 * "Duty %" while the logbook stamped every saved finding with "m:boost" or
 * "m:raw" - names the UI had deliberately stopped using in 3.0, because
 * Boost/Raw read as a quality setting rather than as a scale. The result was a
 * log describing a setting in vocabulary that appears nowhere on the device,
 * which is exactly the kind of thing you cannot act on six months later. */
static const char* const meter_labels[2] = {"0-100", "Duty %"};

static const char* const survey_labels[SPECTER_SURVEY_COUNT] = {"30s", "60s", "2min"};
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
    s->intro = true; // the boot animation, on by default
}

const char* specter_settings_meter_label(uint8_t index) {
    return meter_labels[index & 1u];
}

const char* specter_settings_meter_tag(const SpecterSettings* s) {
    furi_assert(s);
    /* Literally the label the Settings screen shows, so the two can never drift
     * apart again. The logbook writer already scrubs commas and newlines, so a
     * space in "Duty %" cannot shift a CSV column. */
    return specter_settings_meter_label(s->meter_raw ? 1u : 0u);
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

    /* saved_struct checks a magic, a version and a size - it does not and
     * cannot check that the bytes make sense. A _Bool holding anything other
     * than 0 or 1 is undefined behaviour the moment it is read, so a hand-edited
     * or corrupted file could put the app somewhere the language has no answer
     * for. Force them back to a real boolean. */
    s->sound = !!s->sound;
    s->vibro = !!s->vibro;
    s->led = !!s->led;
    s->stealth = !!s->stealth;
    s->logging = !!s->logging;
    s->meter_raw = !!s->meter_raw;
    s->intro = !!s->intro;
}

void specter_settings_load(SpecterSettings* s) {
    furi_assert(s);
    specter_settings_set_defaults(s);

    SpecterSettings loaded;
    if(saved_struct_load(
           SETTINGS_PATH, &loaded, sizeof(loaded), SETTINGS_MAGIC, SETTINGS_VERSION)) {
        specter_settings_sanitise(&loaded);
        *s = loaded;
        return;
    }

    /* Not the current version. Before giving up and handing back defaults, try
     * the previous layout: everything it holds is still meaningful, and the
     * only field it lacks already has its default sitting in *s. The file is
     * rewritten in the new format on the next save, so this path is taken at
     * most once per installation. */
    SpecterSettingsV2 old;
    if(saved_struct_load(SETTINGS_PATH, &old, sizeof(old), SETTINGS_MAGIC, SETTINGS_VERSION_V2)) {
        s->sensitivity_index = old.sensitivity_index;
        s->custom_threshold = old.custom_threshold;
        s->survey_index = old.survey_index;
        s->sound = old.sound;
        s->vibro = old.vibro;
        s->led = old.led;
        s->stealth = old.stealth;
        s->logging = old.logging;
        s->meter_raw = old.meter_raw;
        specter_settings_sanitise(s);
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
