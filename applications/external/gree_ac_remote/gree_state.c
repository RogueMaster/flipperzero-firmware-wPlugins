#include "gree_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define GREE_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} GreeStateStorage;

static void to_storage(const GreeState* s, GreeStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(GreeState* s, const GreeStateStorage* i) {
    s->mode = (GreeMode)i->mode;
    s->last_active_mode = (GreeMode)i->last_active_mode;
    s->fan = (GreeFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

GreeState* gree_state_alloc(void) {
    GreeState* state = malloc(sizeof(GreeState));
    furi_assert(state);
    gree_state_reset(state);
    return state;
}

void gree_state_free(GreeState* state) {
    if(state) free(state);
}

void gree_state_reset(GreeState* state) {
    if(!state) return;
    state->mode = GREE_DEFAULT_MODE;
    state->last_active_mode = GREE_DEFAULT_MODE;
    state->fan = GREE_DEFAULT_FAN;
    state->temp = GREE_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = GREE_DEFAULT_SAVE_STATE;
}

bool gree_state_load(GreeState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, GREE_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        gree_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, GREE_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == GREE_STATE_MAGIC && version == GREE_STATE_FILE_VERSION) {
            GreeStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < GreeModeCount && tmp.last_active_mode < GreeModeCount &&
                   tmp.last_active_mode != GreeModeOff && tmp.fan < GreeFanCount &&
                   tmp.temp >= GREE_TEMP_MIN && tmp.temp <= GREE_TEMP_MAX &&
                   (gree_ir_get_option_count() == 0 || tmp.option < gree_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        gree_state_reset(state);
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

    if(!success) gree_state_reset(state);
    return success;
}

bool gree_state_save(GreeState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, GREE_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = GREE_STATE_MAGIC;
        uint8_t version = GREE_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            GreeStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                GreeState def;
                gree_state_reset(&def);
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

void gree_state_set_mode(GreeState* state, GreeMode mode) {
    if(!state || mode >= GreeModeCount) return;
    state->mode = mode;
    if(mode != GreeModeOff) state->last_active_mode = mode;
}

void gree_state_set_fan(GreeState* state, GreeFan fan) {
    if(!state || fan >= GreeFanCount) return;
    state->fan = fan;
}

void gree_state_temp_up(GreeState* state) {
    if(state && state->temp < GREE_TEMP_MAX) state->temp++;
}

void gree_state_temp_down(GreeState* state) {
    if(state && state->temp > GREE_TEMP_MIN) state->temp--;
}

void gree_state_toggle(GreeState* state, GreeToggle toggle) {
    if(!state || toggle >= GreeToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(gree_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool gree_state_toggle_active(const GreeState* state, GreeToggle toggle) {
    if(!state || toggle >= GreeToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool gree_state_can_change_fan(const GreeState* state) {
    if(!state || state->mode == GreeModeOff) return false;
    return !gree_ir_mode_locks_fan(state->mode);
}

bool gree_state_can_change_temp(const GreeState* state) {
    if(!state || state->mode == GreeModeOff) return false;
    return !gree_ir_mode_has_no_temp(state->mode);
}

GreeRequest gree_state_request(const GreeState* state) {
    GreeRequest req = {GreeModeOff, GreeFanAuto, GREE_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void gree_state_format_current(const GreeState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    GreeRequest req = gree_state_request(state);
    gree_ir_format_state(&req, out, len);
}
