#include "goodweather_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define GOODWEATHER_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} GoodweatherStateStorage;

static void to_storage(const GoodweatherState* s, GoodweatherStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(GoodweatherState* s, const GoodweatherStateStorage* i) {
    s->mode = (GoodweatherMode)i->mode;
    s->last_active_mode = (GoodweatherMode)i->last_active_mode;
    s->fan = (GoodweatherFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

GoodweatherState* goodweather_state_alloc(void) {
    GoodweatherState* state = malloc(sizeof(GoodweatherState));
    furi_assert(state);
    goodweather_state_reset(state);
    return state;
}

void goodweather_state_free(GoodweatherState* state) {
    if(state) free(state);
}

void goodweather_state_reset(GoodweatherState* state) {
    if(!state) return;
    state->mode = GOODWEATHER_DEFAULT_MODE;
    state->last_active_mode = GOODWEATHER_DEFAULT_MODE;
    state->fan = GOODWEATHER_DEFAULT_FAN;
    state->temp = GOODWEATHER_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = GOODWEATHER_DEFAULT_SAVE_STATE;
}

bool goodweather_state_load(GoodweatherState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, GOODWEATHER_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        goodweather_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, GOODWEATHER_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == GOODWEATHER_STATE_MAGIC && version == GOODWEATHER_STATE_FILE_VERSION) {
            GoodweatherStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < GoodweatherModeCount &&
                   tmp.last_active_mode < GoodweatherModeCount &&
                   tmp.last_active_mode != GoodweatherModeOff && tmp.fan < GoodweatherFanCount &&
                   tmp.temp >= GOODWEATHER_TEMP_MIN && tmp.temp <= GOODWEATHER_TEMP_MAX &&
                   (goodweather_ir_get_option_count() == 0 ||
                    tmp.option < goodweather_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        goodweather_state_reset(state);
                        state->save_state = false;
                    }
                    success = true;
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) goodweather_state_reset(state);
    return success;
}

bool goodweather_state_save(GoodweatherState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, GOODWEATHER_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = GOODWEATHER_STATE_MAGIC;
        uint8_t version = GOODWEATHER_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            GoodweatherStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                GoodweatherState def;
                goodweather_state_reset(&def);
                def.save_state = false;
                to_storage(&def, &out);
            }
            success = storage_file_write(file, &out, sizeof(out)) == sizeof(out);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

void goodweather_state_set_mode(GoodweatherState* state, GoodweatherMode mode) {
    if(!state || mode >= GoodweatherModeCount) return;
    state->mode = mode;
    if(mode != GoodweatherModeOff) state->last_active_mode = mode;
}

void goodweather_state_set_fan(GoodweatherState* state, GoodweatherFan fan) {
    if(!state || fan >= GoodweatherFanCount) return;
    state->fan = fan;
}

void goodweather_state_temp_up(GoodweatherState* state) {
    if(state && state->temp < GOODWEATHER_TEMP_MAX) state->temp++;
}

void goodweather_state_temp_down(GoodweatherState* state) {
    if(state && state->temp > GOODWEATHER_TEMP_MIN) state->temp--;
}

void goodweather_state_toggle(GoodweatherState* state, GoodweatherToggle toggle) {
    if(!state || toggle >= GoodweatherToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(goodweather_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool goodweather_state_toggle_active(const GoodweatherState* state, GoodweatherToggle toggle) {
    if(!state || toggle >= GoodweatherToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool goodweather_state_can_change_fan(const GoodweatherState* state) {
    if(!state || state->mode == GoodweatherModeOff) return false;
    return !goodweather_ir_mode_locks_fan(state->mode);
}

bool goodweather_state_can_change_temp(const GoodweatherState* state) {
    if(!state || state->mode == GoodweatherModeOff) return false;
    return !goodweather_ir_mode_has_no_temp(state->mode);
}

GoodweatherRequest goodweather_state_request(const GoodweatherState* state) {
    GoodweatherRequest req = {
        GoodweatherModeOff, GoodweatherFanAuto, GOODWEATHER_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void goodweather_state_format_current(const GoodweatherState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    GoodweatherRequest req = goodweather_state_request(state);
    goodweather_ir_format_state(&req, out, len);
}
