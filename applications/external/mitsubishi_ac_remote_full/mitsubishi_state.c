#include "mitsubishi_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define MITSUBISHI_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} MitsubishiStateStorage;

static void to_storage(const MitsubishiState* s, MitsubishiStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(MitsubishiState* s, const MitsubishiStateStorage* i) {
    s->mode = (MitsubishiMode)i->mode;
    s->last_active_mode = (MitsubishiMode)i->last_active_mode;
    s->fan = (MitsubishiFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

MitsubishiState* mitsubishi_state_alloc(void) {
    MitsubishiState* state = malloc(sizeof(MitsubishiState));
    furi_assert(state);
    mitsubishi_state_reset(state);
    return state;
}

void mitsubishi_state_free(MitsubishiState* state) {
    if(state) free(state);
}

void mitsubishi_state_reset(MitsubishiState* state) {
    if(!state) return;
    state->mode = MITSUBISHI_DEFAULT_MODE;
    state->last_active_mode = MITSUBISHI_DEFAULT_MODE;
    state->fan = MITSUBISHI_DEFAULT_FAN;
    state->temp = MITSUBISHI_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = MITSUBISHI_DEFAULT_SAVE_STATE;
}

bool mitsubishi_state_load(MitsubishiState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, MITSUBISHI_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        mitsubishi_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, MITSUBISHI_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == MITSUBISHI_STATE_MAGIC && version == MITSUBISHI_STATE_FILE_VERSION) {
            MitsubishiStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < MitsubishiModeCount && tmp.last_active_mode < MitsubishiModeCount &&
                   tmp.last_active_mode != MitsubishiModeOff && tmp.fan < MitsubishiFanCount &&
                   tmp.temp >= MITSUBISHI_TEMP_MIN && tmp.temp <= MITSUBISHI_TEMP_MAX &&
                   (mitsubishi_ir_get_option_count() == 0 ||
                    tmp.option < mitsubishi_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        mitsubishi_state_reset(state);
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

    if(!success) mitsubishi_state_reset(state);
    return success;
}

bool mitsubishi_state_save(MitsubishiState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, MITSUBISHI_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = MITSUBISHI_STATE_MAGIC;
        uint8_t version = MITSUBISHI_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            MitsubishiStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                MitsubishiState def;
                mitsubishi_state_reset(&def);
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

void mitsubishi_state_set_mode(MitsubishiState* state, MitsubishiMode mode) {
    if(!state || mode >= MitsubishiModeCount) return;
    state->mode = mode;
    if(mode != MitsubishiModeOff) state->last_active_mode = mode;
}

void mitsubishi_state_set_fan(MitsubishiState* state, MitsubishiFan fan) {
    if(!state || fan >= MitsubishiFanCount) return;
    state->fan = fan;
}

void mitsubishi_state_temp_up(MitsubishiState* state) {
    if(state && state->temp < MITSUBISHI_TEMP_MAX) state->temp++;
}

void mitsubishi_state_temp_down(MitsubishiState* state) {
    if(state && state->temp > MITSUBISHI_TEMP_MIN) state->temp--;
}

void mitsubishi_state_toggle(MitsubishiState* state, MitsubishiToggle toggle) {
    if(!state || toggle >= MitsubishiToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(mitsubishi_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool mitsubishi_state_toggle_active(const MitsubishiState* state, MitsubishiToggle toggle) {
    if(!state || toggle >= MitsubishiToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool mitsubishi_state_can_change_fan(const MitsubishiState* state) {
    if(!state || state->mode == MitsubishiModeOff) return false;
    return !mitsubishi_ir_mode_locks_fan(state->mode);
}

bool mitsubishi_state_can_change_temp(const MitsubishiState* state) {
    if(!state || state->mode == MitsubishiModeOff) return false;
    return !mitsubishi_ir_mode_has_no_temp(state->mode);
}

MitsubishiRequest mitsubishi_state_request(const MitsubishiState* state) {
    MitsubishiRequest req = {MitsubishiModeOff, MitsubishiFanAuto, MITSUBISHI_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void mitsubishi_state_format_current(const MitsubishiState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    MitsubishiRequest req = mitsubishi_state_request(state);
    mitsubishi_ir_format_state(&req, out, len);
}
