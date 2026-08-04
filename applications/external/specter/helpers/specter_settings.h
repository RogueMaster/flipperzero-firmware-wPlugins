#pragma once

#include <stdbool.h>
#include <stdint.h>

/* User settings, persisted to the SD card so a sweep kit stays configured the
 * way you left it. Loading is defensive: a truncated or hand-edited file must
 * never produce an out-of-range index that indexes a label table. */

#define SPECTER_SENS_COUNT   4u // High / Medium / Low / Custom
#define SPECTER_SENS_CUSTOM  3u
#define SPECTER_SURVEY_COUNT 3u // 30 s / 60 s / 2 min

typedef struct {
    uint8_t sensitivity_index; // 0 High, 1 Medium, 2 Low, 3 Custom
    uint8_t custom_threshold; // duty-cycle floor used when Custom is selected
    uint8_t survey_index; // site-survey duration
    bool sound;
    bool vibro;
    bool led;
    bool stealth; // screen + LED dark: sweep without advertising that you are
    bool logging; // append findings to the SD logbook
    bool meter_raw; // show unscaled carrier duty instead of a full-scale meter
} SpecterSettings;

void specter_settings_set_defaults(SpecterSettings* s);

/* Load from SD, falling back to defaults on a missing or invalid file. */
void specter_settings_load(SpecterSettings* s);
bool specter_settings_save(const SpecterSettings* s);

/* Duty-cycle noise floor implied by the current sensitivity. */
uint8_t specter_settings_threshold(const SpecterSettings* s);

/* Raw duty that should read as a full meter, per the Meter setting. */
uint8_t specter_settings_full_scale(const SpecterSettings* s);

const char* specter_settings_sensitivity_label(uint8_t index);
const char* specter_settings_survey_label(uint8_t index);
uint32_t specter_settings_survey_seconds(const SpecterSettings* s);
