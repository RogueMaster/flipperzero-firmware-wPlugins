#ifndef SETTINGS_H
#define SETTINGS_H

#include "internet_time.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

/** Explicit SD path — reliable on stock and common CFW. */
#define SETTINGS_CONFIG_PATH EXT_PATH("apps_data/internet_time_screen/config.txt")

/** Number of selectable quarter-hour offsets from UTC-12 to UTC+14. */
#define SETTINGS_OFFSET_COUNT                                                 \
    (((INTERNET_TIME_OFFSET_MAX_MINUTES - INTERNET_TIME_OFFSET_MIN_MINUTES) / \
      INTERNET_TIME_OFFSET_STEP_MINUTES) +                                    \
     1)

typedef struct {
    int16_t utc_offset_minutes;
    bool loaded;
} AppSettings;

void settings_init_defaults(AppSettings* settings);
bool settings_load(Storage* storage, AppSettings* settings);
bool settings_save(Storage* storage, const AppSettings* settings);
uint8_t settings_offset_to_index(int16_t utc_offset_minutes);
int16_t settings_index_to_offset(uint8_t index);
void settings_format_offset(int16_t utc_offset_minutes, char* out, size_t out_size);

#endif /* SETTINGS_H */
