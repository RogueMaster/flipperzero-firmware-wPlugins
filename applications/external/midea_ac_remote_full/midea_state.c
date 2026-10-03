#include "midea_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define MIDEA_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} MideaStateStorage;

static void to_storage(const MideaState* s, MideaStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(MideaState* s, const MideaStateStorage* i) {
    s->mode = (MideaMode)i->mode;
    s->last_active_mode = (MideaMode)i->last_active_mode;
    s->fan = (MideaFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

MideaState* midea_state_alloc(void) {
    MideaState* state = malloc(sizeof(MideaState));
    furi_assert(state);
    midea_state_reset(state);
    return state;
}

void midea_state_free(MideaState* state) {
    if(state) free(state);
}

void midea_state_reset(MideaState* state) {
    if(!state) return;
    state->mode = MIDEA_DEFAULT_MODE;
    state->last_active_mode = MIDEA_DEFAULT_MODE;
    state->fan = MIDEA_DEFAULT_FAN;
    state->temp = MIDEA_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = MIDEA_DEFAULT_SAVE_STATE;
}

bool midea_state_load(MideaState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, MIDEA_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        midea_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, MIDEA_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == MIDEA_STATE_MAGIC && version == MIDEA_STATE_FILE_VERSION) {
            MideaStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < MideaModeCount && tmp.last_active_mode < MideaModeCount &&
                   tmp.last_active_mode != MideaModeOff && tmp.fan < MideaFanCount &&
                   tmp.temp >= MIDEA_TEMP_MIN && tmp.temp <= MIDEA_TEMP_MAX &&
                   (midea_ir_get_option_count() == 0 ||
                    tmp.option < midea_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        midea_state_reset(state);
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

    if(!success) midea_state_reset(state);
    return success;
}

bool midea_state_save(MideaState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, MIDEA_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = MIDEA_STATE_MAGIC;
        uint8_t version = MIDEA_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            MideaStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                MideaState def;
                midea_state_reset(&def);
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

void midea_state_set_mode(MideaState* state, MideaMode mode) {
    if(!state || mode >= MideaModeCount) return;
    state->mode = mode;
    if(mode != MideaModeOff) state->last_active_mode = mode;
}

void midea_state_set_fan(MideaState* state, MideaFan fan) {
    if(!state || fan >= MideaFanCount) return;
    state->fan = fan;
}

void midea_state_temp_up(MideaState* state) {
    if(state && state->temp < MIDEA_TEMP_MAX) state->temp++;
}

void midea_state_temp_down(MideaState* state) {
    if(state && state->temp > MIDEA_TEMP_MIN) state->temp--;
}

void midea_state_toggle(MideaState* state, MideaToggle toggle) {
    if(!state || toggle >= MideaToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(midea_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool midea_state_toggle_active(const MideaState* state, MideaToggle toggle) {
    if(!state || toggle >= MideaToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool midea_state_can_change_fan(const MideaState* state) {
    if(!state || state->mode == MideaModeOff) return false;
    return !midea_ir_mode_locks_fan(state->mode);
}

bool midea_state_can_change_temp(const MideaState* state) {
    if(!state || state->mode == MideaModeOff) return false;
    return !midea_ir_mode_has_no_temp(state->mode);
}

MideaRequest midea_state_request(const MideaState* state) {
    MideaRequest req = {MideaModeOff, MideaFanAuto, MIDEA_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void midea_state_format_current(const MideaState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    MideaRequest req = midea_state_request(state);
    midea_ir_format_state(&req, out, len);
}
