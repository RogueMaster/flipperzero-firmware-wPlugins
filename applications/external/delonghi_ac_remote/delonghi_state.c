#include "delonghi_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define DELONGHI_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} DelonghiStateStorage;

static void to_storage(const DelonghiState* s, DelonghiStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(DelonghiState* s, const DelonghiStateStorage* i) {
    s->mode = (DelonghiMode)i->mode;
    s->last_active_mode = (DelonghiMode)i->last_active_mode;
    s->fan = (DelonghiFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

DelonghiState* delonghi_state_alloc(void) {
    DelonghiState* state = malloc(sizeof(DelonghiState));
    furi_assert(state);
    delonghi_state_reset(state);
    return state;
}

void delonghi_state_free(DelonghiState* state) {
    if(state) free(state);
}

void delonghi_state_reset(DelonghiState* state) {
    if(!state) return;
    state->mode = DELONGHI_DEFAULT_MODE;
    state->last_active_mode = DELONGHI_DEFAULT_MODE;
    state->fan = DELONGHI_DEFAULT_FAN;
    state->temp = DELONGHI_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = DELONGHI_DEFAULT_SAVE_STATE;
}

bool delonghi_state_load(DelonghiState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, DELONGHI_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        delonghi_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, DELONGHI_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == DELONGHI_STATE_MAGIC && version == DELONGHI_STATE_FILE_VERSION) {
            DelonghiStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < DelonghiModeCount && tmp.last_active_mode < DelonghiModeCount &&
                   tmp.last_active_mode != DelonghiModeOff && tmp.fan < DelonghiFanCount &&
                   tmp.temp >= DELONGHI_TEMP_MIN && tmp.temp <= DELONGHI_TEMP_MAX &&
                   (delonghi_ir_get_option_count() == 0 ||
                    tmp.option < delonghi_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        delonghi_state_reset(state);
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

    if(!success) delonghi_state_reset(state);
    return success;
}

bool delonghi_state_save(DelonghiState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, DELONGHI_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = DELONGHI_STATE_MAGIC;
        uint8_t version = DELONGHI_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            DelonghiStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                DelonghiState def;
                delonghi_state_reset(&def);
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

void delonghi_state_set_mode(DelonghiState* state, DelonghiMode mode) {
    if(!state || mode >= DelonghiModeCount) return;
    state->mode = mode;
    if(mode != DelonghiModeOff) state->last_active_mode = mode;
}

void delonghi_state_set_fan(DelonghiState* state, DelonghiFan fan) {
    if(!state || fan >= DelonghiFanCount) return;
    state->fan = fan;
}

void delonghi_state_temp_up(DelonghiState* state) {
    if(state && state->temp < DELONGHI_TEMP_MAX) state->temp++;
}

void delonghi_state_temp_down(DelonghiState* state) {
    if(state && state->temp > DELONGHI_TEMP_MIN) state->temp--;
}

void delonghi_state_toggle(DelonghiState* state, DelonghiToggle toggle) {
    if(!state || toggle >= DelonghiToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(delonghi_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool delonghi_state_toggle_active(const DelonghiState* state, DelonghiToggle toggle) {
    if(!state || toggle >= DelonghiToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool delonghi_state_can_change_fan(const DelonghiState* state) {
    if(!state || state->mode == DelonghiModeOff) return false;
    return !delonghi_ir_mode_locks_fan(state->mode);
}

bool delonghi_state_can_change_temp(const DelonghiState* state) {
    if(!state || state->mode == DelonghiModeOff) return false;
    return !delonghi_ir_mode_has_no_temp(state->mode);
}

DelonghiRequest delonghi_state_request(const DelonghiState* state) {
    DelonghiRequest req = {DelonghiModeOff, DelonghiFanAuto, DELONGHI_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void delonghi_state_format_current(const DelonghiState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    DelonghiRequest req = delonghi_state_request(state);
    delonghi_ir_format_state(&req, out, len);
}
