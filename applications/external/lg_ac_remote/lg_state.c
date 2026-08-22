#include "lg_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define LG_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} LgStateStorage;

static void to_storage(const LgState* s, LgStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(LgState* s, const LgStateStorage* i) {
    s->mode = (LgMode)i->mode;
    s->last_active_mode = (LgMode)i->last_active_mode;
    s->fan = (LgFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

LgState* lg_state_alloc(void) {
    LgState* state = malloc(sizeof(LgState));
    furi_assert(state);
    lg_state_reset(state);
    return state;
}

void lg_state_free(LgState* state) {
    if(state) free(state);
}

void lg_state_reset(LgState* state) {
    if(!state) return;
    state->mode = LG_DEFAULT_MODE;
    state->last_active_mode = LG_DEFAULT_MODE;
    state->fan = LG_DEFAULT_FAN;
    state->temp = LG_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = LG_DEFAULT_SAVE_STATE;
}

bool lg_state_load(LgState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, LG_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        lg_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, LG_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == LG_STATE_MAGIC && version == LG_STATE_FILE_VERSION) {
            LgStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < LgModeCount && tmp.last_active_mode < LgModeCount &&
                   tmp.last_active_mode != LgModeOff && tmp.fan < LgFanCount &&
                   tmp.temp >= LG_TEMP_MIN && tmp.temp <= LG_TEMP_MAX &&
                   (lg_ir_get_option_count() == 0 || tmp.option < lg_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        lg_state_reset(state);
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

    if(!success) lg_state_reset(state);
    return success;
}

bool lg_state_save(LgState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, LG_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = LG_STATE_MAGIC;
        uint8_t version = LG_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            LgStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                LgState def;
                lg_state_reset(&def);
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

void lg_state_set_mode(LgState* state, LgMode mode) {
    if(!state || mode >= LgModeCount) return;
    state->mode = mode;
    if(mode != LgModeOff) state->last_active_mode = mode;
}

void lg_state_set_fan(LgState* state, LgFan fan) {
    if(!state || fan >= LgFanCount) return;
    state->fan = fan;
}

void lg_state_temp_up(LgState* state) {
    if(state && state->temp < LG_TEMP_MAX) state->temp++;
}

void lg_state_temp_down(LgState* state) {
    if(state && state->temp > LG_TEMP_MIN) state->temp--;
}

void lg_state_toggle(LgState* state, LgToggle toggle) {
    if(!state || toggle >= LgToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(lg_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool lg_state_toggle_active(const LgState* state, LgToggle toggle) {
    if(!state || toggle >= LgToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool lg_state_can_change_fan(const LgState* state) {
    if(!state || state->mode == LgModeOff) return false;
    return !lg_ir_mode_locks_fan(state->mode);
}

bool lg_state_can_change_temp(const LgState* state) {
    if(!state || state->mode == LgModeOff) return false;
    return !lg_ir_mode_has_no_temp(state->mode);
}

LgRequest lg_state_request(const LgState* state) {
    LgRequest req = {LgModeOff, LgFanAuto, LG_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void lg_state_format_current(const LgState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    LgRequest req = lg_state_request(state);
    lg_ir_format_state(&req, out, len);
}
