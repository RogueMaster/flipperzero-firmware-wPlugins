#include "daikin_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define DAIKIN_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} DaikinStateStorage;

static void to_storage(const DaikinState* s, DaikinStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(DaikinState* s, const DaikinStateStorage* i) {
    s->mode = (DaikinMode)i->mode;
    s->last_active_mode = (DaikinMode)i->last_active_mode;
    s->fan = (DaikinFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

DaikinState* daikin_state_alloc(void) {
    DaikinState* state = malloc(sizeof(DaikinState));
    furi_assert(state);
    daikin_state_reset(state);
    return state;
}

void daikin_state_free(DaikinState* state) {
    if(state) free(state);
}

void daikin_state_reset(DaikinState* state) {
    if(!state) return;
    state->mode = DAIKIN_DEFAULT_MODE;
    state->last_active_mode = DAIKIN_DEFAULT_MODE;
    state->fan = DAIKIN_DEFAULT_FAN;
    state->temp = DAIKIN_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = DAIKIN_DEFAULT_SAVE_STATE;
}

bool daikin_state_load(DaikinState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, DAIKIN_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        daikin_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, DAIKIN_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == DAIKIN_STATE_MAGIC && version == DAIKIN_STATE_FILE_VERSION) {
            DaikinStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < DaikinModeCount && tmp.last_active_mode < DaikinModeCount &&
                   tmp.last_active_mode != DaikinModeOff && tmp.fan < DaikinFanCount &&
                   tmp.temp >= DAIKIN_TEMP_MIN && tmp.temp <= DAIKIN_TEMP_MAX &&
                   (daikin_ir_get_option_count() == 0 ||
                    tmp.option < daikin_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        daikin_state_reset(state);
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

    if(!success) daikin_state_reset(state);
    return success;
}

bool daikin_state_save(DaikinState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, DAIKIN_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = DAIKIN_STATE_MAGIC;
        uint8_t version = DAIKIN_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            DaikinStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                DaikinState def;
                daikin_state_reset(&def);
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

void daikin_state_set_mode(DaikinState* state, DaikinMode mode) {
    if(!state || mode >= DaikinModeCount) return;
    state->mode = mode;
    if(mode != DaikinModeOff) state->last_active_mode = mode;
}

void daikin_state_set_fan(DaikinState* state, DaikinFan fan) {
    if(!state || fan >= DaikinFanCount) return;
    state->fan = fan;
}

void daikin_state_temp_up(DaikinState* state) {
    if(state && state->temp < DAIKIN_TEMP_MAX) state->temp++;
}

void daikin_state_temp_down(DaikinState* state) {
    if(state && state->temp > DAIKIN_TEMP_MIN) state->temp--;
}

void daikin_state_toggle(DaikinState* state, DaikinToggle toggle) {
    if(!state || toggle >= DaikinToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(daikin_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool daikin_state_toggle_active(const DaikinState* state, DaikinToggle toggle) {
    if(!state || toggle >= DaikinToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool daikin_state_can_change_fan(const DaikinState* state) {
    if(!state || state->mode == DaikinModeOff) return false;
    return !daikin_ir_mode_locks_fan(state->mode);
}

bool daikin_state_can_change_temp(const DaikinState* state) {
    if(!state || state->mode == DaikinModeOff) return false;
    return !daikin_ir_mode_has_no_temp(state->mode);
}

DaikinRequest daikin_state_request(const DaikinState* state) {
    DaikinRequest req = {DaikinModeOff, DaikinFanAuto, DAIKIN_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void daikin_state_format_current(const DaikinState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    DaikinRequest req = daikin_state_request(state);
    daikin_ir_format_state(&req, out, len);
}
