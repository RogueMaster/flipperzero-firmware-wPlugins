#pragma once
#include <stdint.h>
#include <stdbool.h>

#define BAUD_RATE_COUNT 6
extern const uint32_t BAUD_RATES[BAUD_RATE_COUNT];
extern const char* const BAUD_LABELS[BAUD_RATE_COUNT];

#define TRACK_INTERVAL_COUNT 5
extern const uint8_t TRACK_INTERVALS_S[TRACK_INTERVAL_COUNT];
extern const char* const TRACK_INTERVAL_LABELS[TRACK_INTERVAL_COUNT];

#define TRACK_MIN_DIST_COUNT 4
extern const uint8_t TRACK_MIN_DISTS_M[TRACK_MIN_DIST_COUNT];
extern const char* const TRACK_MIN_DIST_LABELS[TRACK_MIN_DIST_COUNT];

#define DUPLICATE_CHECK_COUNT 4
extern const uint8_t DUPLICATE_CHECK_M[DUPLICATE_CHECK_COUNT];
extern const char* const DUPLICATE_CHECK_LABELS[DUPLICATE_CHECK_COUNT];

#define AVG_SECONDS_COUNT 5
extern const uint8_t AVG_SECONDS[AVG_SECONDS_COUNT];
extern const char* const AVG_SECONDS_LABELS[AVG_SECONDS_COUNT];

// HDOP max stocké en dixièmes (0 = désactivé, sinon 25 = 2.5, 30 = 3.0, ...)
#define HDOP_MAX_COUNT 5
extern const uint8_t HDOP_MAX_X10[HDOP_MAX_COUNT];
extern const char* const HDOP_MAX_LABELS[HDOP_MAX_COUNT];

typedef struct {
    uint32_t baud_rate;
    uint8_t track_interval_s;
    uint8_t track_min_dist_m;  // 0 = désactivé
    bool track_hdop_strict;
    bool preview_before_save;
    bool auto_photo_id;
    uint8_t duplicate_check_m; // 0 = désactivé
    uint8_t avg_seconds;       // 0 = instantané, sinon moyenne sur N secondes
    uint8_t hdop_max_x10;      // 0 = gate désactivé, sinon HDOP max × 10 (ex. 50 = 5.0)
    bool survey_mode;          // ajoute source=survey + survey:date=YYYY-MM-DD à chaque save
} Settings;

void settings_defaults(Settings* s);
void settings_load(Settings* s);
void settings_save(const Settings* s);

uint8_t baud_rate_to_idx(uint32_t baud);
uint8_t track_interval_to_idx(uint8_t seconds);
uint8_t track_min_dist_to_idx(uint8_t meters);
uint8_t duplicate_check_to_idx(uint8_t meters);
uint8_t avg_seconds_to_idx(uint8_t seconds);
uint8_t hdop_max_to_idx(uint8_t hdop_x10);

#ifdef __cplusplus
extern "C" {
#endif

typedef struct App App;
void app_start_settings(App* app);
void app_stop_settings(App* app);

#ifdef __cplusplus
}
#endif
