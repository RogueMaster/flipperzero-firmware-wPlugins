#include "rc_settings.h"

#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#define TAG "RollCall"

#define RC_SETTINGS_PATH    APP_DATA_PATH("rollcall.conf")
#define RC_SETTINGS_MAGIC   0x5C
#define RC_SETTINGS_VERSION 1

const RcGap rc_gaps[RC_GAP_COUNT] = {
    {.label = "0.25s", .ms = 250},
    {.label = "0.5s", .ms = 500},
    {.label = "0.75s", .ms = 750},
    {.label = "1.0s", .ms = 1000},
    {.label = "1.5s", .ms = 1500},
};

void rc_settings_default(RcSettings* s) {
    furi_assert(s);
    s->band_idx = RC_BAND_DEFAULT;
    s->mod_idx = 0; // AM650 covers the large majority of fobs
    s->target = 3;
    s->gap_idx = 1; // 0.5s
    s->sound = true;
    s->vibro = true;
    s->led = true;
}

/* A hand-edited or half-written file must never be able to index off the end
 * of rc_bands / rc_mods / rc_gaps, so every field is re-checked on load. */
static bool rc_settings_valid(const RcSettings* s) {
    return s->band_idx < RC_BAND_COUNT && s->mod_idx < RC_MOD_COUNT &&
           s->gap_idx < RC_GAP_COUNT && s->target >= RC_TARGET_MIN &&
           s->target <= RC_TARGET_MAX;
}

void rc_settings_load(RcSettings* s) {
    furi_assert(s);
    rc_settings_default(s);

    RcSettings loaded;
    if(!saved_struct_load(
           RC_SETTINGS_PATH,
           &loaded,
           sizeof(RcSettings),
           RC_SETTINGS_MAGIC,
           RC_SETTINGS_VERSION)) {
        FURI_LOG_D(TAG, "no saved settings, using defaults");
        return;
    }

    if(!rc_settings_valid(&loaded)) {
        FURI_LOG_W(TAG, "saved settings out of range, using defaults");
        return;
    }

    *s = loaded;
}

void rc_settings_save(const RcSettings* s) {
    furi_assert(s);

    /* The app data directory is created lazily; make sure it is there before
     * the first write, otherwise saved_struct_save has nowhere to land. */
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, STORAGE_APP_DATA_PATH_PREFIX);
    furi_record_close(RECORD_STORAGE);

    if(!saved_struct_save(
           RC_SETTINGS_PATH,
           s,
           sizeof(RcSettings),
           RC_SETTINGS_MAGIC,
           RC_SETTINGS_VERSION)) {
        FURI_LOG_W(TAG, "could not save settings");
    }
}
