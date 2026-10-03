#pragma once

#include <furi.h>

#define BM_SETTINGS_PATH    APP_DATA_PATH("bettermouse.settings")
#define BM_SETTINGS_MAGIC   0xB3
#define BM_SETTINGS_VERSION 3

/* Raw values, not indices into a table of presets. Nothing here is clamped to
   a "sensible" range on purpose: dial in whatever you like. The only hard
   ceiling is physics, see the note in bm_mouse.c. */
typedef struct {
    uint16_t max_speed; /* px/s at the top of the ramp */
    uint16_t accel_ms; /* time to reach max_speed, 0 = instant */
    uint16_t min_speed; /* px/s before the ramp starts building */
    uint8_t scroll_speed; /* wheel notches per second */
    uint16_t tick_hz; /* motion loop and report rate */

    uint8_t ice; /* momentum mode on/off */
    uint16_t ice_glide_ms; /* coast half-life once you let go */

    uint8_t invert_y;
    uint8_t invert_scroll;
    uint8_t haptic;
} BmSettings;

/* Version 2 on disk, kept only so an existing file can be carried forward
   instead of being thrown away when the layout changes. */
typedef struct {
    uint16_t max_speed;
    uint16_t accel_ms;
    uint8_t fine_div;
    uint8_t scroll_speed;
    uint16_t tick_hz;
    uint8_t ice;
    uint16_t ice_glide_ms;
    uint8_t invert_y;
    uint8_t invert_scroll;
    uint8_t haptic;
} BmSettingsV2;

/* Each tunable is a (min, step, count) range. count stays under 256 because
   VariableItemList takes a uint8_t value count, so where a range needs to be
   wide the step gets coarser rather than the range getting cut short. */
typedef struct {
    uint16_t min;
    uint16_t step;
    uint8_t count;
} BmRange;

extern const BmRange bm_range_max_speed;
extern const BmRange bm_range_min_speed;
extern const BmRange bm_range_accel;
extern const BmRange bm_range_scroll;
extern const BmRange bm_range_tick;
extern const BmRange bm_range_ice_glide;

extern const char* const bm_toggle_names[];

uint16_t bm_range_value(const BmRange* range, uint8_t index);
uint8_t bm_range_index(const BmRange* range, uint16_t value);

void bm_settings_set_default(BmSettings* settings);
void bm_settings_load(BmSettings* settings);
void bm_settings_save(const BmSettings* settings);
