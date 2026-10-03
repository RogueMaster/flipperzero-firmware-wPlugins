#include "bm_settings.h"

#include <storage/storage.h>
#include <toolbox/saved_struct.h>

/* Wide open on purpose. The top of the speed range is far past what is useful,
   and the ramp goes all the way to "never gets there" -- that is the point. */
const BmRange bm_range_max_speed = {.min = 50, .step = 50, .count = 255}; /* 50..12750 px/s */
const BmRange bm_range_min_speed = {.min = 0, .step = 10, .count = 251}; /* 0..2500 px/s */
const BmRange bm_range_accel = {.min = 0, .step = 20, .count = 251}; /* 0..5000 ms */
const BmRange bm_range_scroll = {.min = 5, .step = 1, .count = 251}; /* 5..255 notches/s */
const BmRange bm_range_tick = {.min = 10, .step = 10, .count = 100}; /* 10..1000 Hz */
const BmRange bm_range_ice_glide = {.min = 10, .step = 10, .count = 200}; /* 10..2000 ms */

const char* const bm_toggle_names[] = {"OFF", "ON"};

uint16_t bm_range_value(const BmRange* range, uint8_t index) {
    if(index >= range->count) index = (uint8_t)(range->count - 1);
    return (uint16_t)(range->min + (uint32_t)index * range->step);
}

uint8_t bm_range_index(const BmRange* range, uint16_t value) {
    if(value <= range->min) return 0;
    uint32_t index = ((uint32_t)value - range->min) / range->step;
    if(index >= range->count) index = (uint32_t)(range->count - 1);
    return (uint8_t)index;
}

void bm_settings_set_default(BmSettings* settings) {
    furi_assert(settings);
    settings->max_speed = 1200;
    settings->accel_ms = 400;
    settings->min_speed = 60;
    settings->scroll_speed = 8;
    settings->tick_hz = 50;
    settings->ice = 0;
    settings->ice_glide_ms = 300;
    settings->invert_y = 0;
    settings->invert_scroll = 0;
    settings->haptic = 1;
}

/* Only guard against values that would divide by zero or stall the motion
   thread. Everything else is yours to break. */
static void bm_settings_sanity(BmSettings* settings) {
    if(settings->scroll_speed < 5) settings->scroll_speed = 5;
    if(settings->tick_hz < 10) settings->tick_hz = 10;
    if(settings->tick_hz > 1000) settings->tick_hz = 1000;
    if(settings->ice_glide_ms < 1) settings->ice_glide_ms = 1;
    settings->ice = settings->ice ? 1 : 0;
    settings->invert_y = settings->invert_y ? 1 : 0;
    settings->invert_scroll = settings->invert_scroll ? 1 : 0;
    settings->haptic = settings->haptic ? 1 : 0;
}

/* Carry a version 2 file forward rather than discarding hard won tuning. The
   only field v2 lacks is min_speed, which takes the default. */
static bool bm_settings_load_v2(BmSettings* settings) {
    BmSettingsV2 old;
    if(!saved_struct_load(BM_SETTINGS_PATH, &old, sizeof(old), BM_SETTINGS_MAGIC, 2)) {
        return false;
    }

    bm_settings_set_default(settings);
    settings->max_speed = old.max_speed;
    settings->accel_ms = old.accel_ms;
    settings->scroll_speed = old.scroll_speed;
    settings->tick_hz = old.tick_hz;
    settings->ice = old.ice;
    settings->ice_glide_ms = old.ice_glide_ms;
    settings->invert_y = old.invert_y;
    settings->invert_scroll = old.invert_scroll;
    settings->haptic = old.haptic;
    return true;
}

void bm_settings_load(BmSettings* settings) {
    furi_assert(settings);

    if(!saved_struct_load(
           BM_SETTINGS_PATH, settings, sizeof(BmSettings), BM_SETTINGS_MAGIC, BM_SETTINGS_VERSION)) {
        if(!bm_settings_load_v2(settings)) {
            bm_settings_set_default(settings);
            return;
        }
    }

    bm_settings_sanity(settings);
}

void bm_settings_save(const BmSettings* settings) {
    furi_assert(settings);
    /* saved_struct_save cannot create the app data directory itself. */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);

    saved_struct_save(
        BM_SETTINGS_PATH, settings, sizeof(BmSettings), BM_SETTINGS_MAGIC, BM_SETTINGS_VERSION);
}
