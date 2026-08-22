#include "neoclima_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define NEOCLIMA_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} NeoclimaStateStorage;

static void to_storage(const NeoclimaState* s, NeoclimaStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(NeoclimaState* s, const NeoclimaStateStorage* i) {
    s->mode = (NeoclimaMode)i->mode;
    s->last_active_mode = (NeoclimaMode)i->last_active_mode;
    s->fan = (NeoclimaFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

NeoclimaState* neoclima_state_alloc(void) {
    NeoclimaState* state = malloc(sizeof(NeoclimaState));
    furi_assert(state);
    neoclima_state_reset(state);
    return state;
}

void neoclima_state_free(NeoclimaState* state) {
    if(state) free(state);
}

void neoclima_state_reset(NeoclimaState* state) {
    if(!state) return;
    state->mode = NEOCLIMA_DEFAULT_MODE;
    state->last_active_mode = NEOCLIMA_DEFAULT_MODE;
    state->fan = NEOCLIMA_DEFAULT_FAN;
    state->temp = NEOCLIMA_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = NEOCLIMA_DEFAULT_SAVE_STATE;
}

bool neoclima_state_load(NeoclimaState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, NEOCLIMA_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        neoclima_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, NEOCLIMA_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == NEOCLIMA_STATE_MAGIC && version == NEOCLIMA_STATE_FILE_VERSION) {
            NeoclimaStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < NeoclimaModeCount && tmp.last_active_mode < NeoclimaModeCount &&
                   tmp.last_active_mode != NeoclimaModeOff && tmp.fan < NeoclimaFanCount &&
                   tmp.temp >= NEOCLIMA_TEMP_MIN && tmp.temp <= NEOCLIMA_TEMP_MAX &&
                   (neoclima_ir_get_option_count() == 0 ||
                    tmp.option < neoclima_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        neoclima_state_reset(state);
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

    if(!success) neoclima_state_reset(state);
    return success;
}

bool neoclima_state_save(NeoclimaState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, NEOCLIMA_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = NEOCLIMA_STATE_MAGIC;
        uint8_t version = NEOCLIMA_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            NeoclimaStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                NeoclimaState def;
                neoclima_state_reset(&def);
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

void neoclima_state_set_mode(NeoclimaState* state, NeoclimaMode mode) {
    if(!state || mode >= NeoclimaModeCount) return;
    state->mode = mode;
    if(mode != NeoclimaModeOff) state->last_active_mode = mode;
}

void neoclima_state_set_fan(NeoclimaState* state, NeoclimaFan fan) {
    if(!state || fan >= NeoclimaFanCount) return;
    state->fan = fan;
}

void neoclima_state_temp_up(NeoclimaState* state) {
    if(state && state->temp < NEOCLIMA_TEMP_MAX) state->temp++;
}

void neoclima_state_temp_down(NeoclimaState* state) {
    if(state && state->temp > NEOCLIMA_TEMP_MIN) state->temp--;
}

void neoclima_state_toggle(NeoclimaState* state, NeoclimaToggle toggle) {
    if(!state || toggle >= NeoclimaToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(neoclima_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool neoclima_state_toggle_active(const NeoclimaState* state, NeoclimaToggle toggle) {
    if(!state || toggle >= NeoclimaToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool neoclima_state_can_change_fan(const NeoclimaState* state) {
    if(!state || state->mode == NeoclimaModeOff) return false;
    return !neoclima_ir_mode_locks_fan(state->mode);
}

bool neoclima_state_can_change_temp(const NeoclimaState* state) {
    if(!state || state->mode == NeoclimaModeOff) return false;
    return !neoclima_ir_mode_has_no_temp(state->mode);
}

NeoclimaRequest neoclima_state_request(const NeoclimaState* state) {
    NeoclimaRequest req = {NeoclimaModeOff, NeoclimaFanAuto, NEOCLIMA_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void neoclima_state_format_current(const NeoclimaState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    NeoclimaRequest req = neoclima_state_request(state);
    neoclima_ir_format_state(&req, out, len);
}
