#include "ballu_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define BALLU_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} BalluStateStorage;

static void to_storage(const BalluState* s, BalluStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(BalluState* s, const BalluStateStorage* i) {
    s->mode = (BalluMode)i->mode;
    s->last_active_mode = (BalluMode)i->last_active_mode;
    s->fan = (BalluFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

BalluState* ballu_state_alloc(void) {
    BalluState* state = malloc(sizeof(BalluState));
    furi_assert(state);
    ballu_state_reset(state);
    return state;
}

void ballu_state_free(BalluState* state) {
    if(state) free(state);
}

void ballu_state_reset(BalluState* state) {
    if(!state) return;
    state->mode = BALLU_DEFAULT_MODE;
    state->last_active_mode = BALLU_DEFAULT_MODE;
    state->fan = BALLU_DEFAULT_FAN;
    state->temp = BALLU_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = BALLU_DEFAULT_SAVE_STATE;
}

bool ballu_state_load(BalluState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, BALLU_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        ballu_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, BALLU_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == BALLU_STATE_MAGIC && version == BALLU_STATE_FILE_VERSION) {
            BalluStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < BalluModeCount && tmp.last_active_mode < BalluModeCount &&
                   tmp.last_active_mode != BalluModeOff && tmp.fan < BalluFanCount &&
                   tmp.temp >= BALLU_TEMP_MIN && tmp.temp <= BALLU_TEMP_MAX &&
                   (ballu_ir_get_option_count() == 0 ||
                    tmp.option < ballu_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        ballu_state_reset(state);
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

    if(!success) ballu_state_reset(state);
    return success;
}

bool ballu_state_save(BalluState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, BALLU_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = BALLU_STATE_MAGIC;
        uint8_t version = BALLU_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            BalluStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                BalluState def;
                ballu_state_reset(&def);
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

void ballu_state_set_mode(BalluState* state, BalluMode mode) {
    if(!state || mode >= BalluModeCount) return;
    state->mode = mode;
    if(mode != BalluModeOff) state->last_active_mode = mode;
}

void ballu_state_set_fan(BalluState* state, BalluFan fan) {
    if(!state || fan >= BalluFanCount) return;
    state->fan = fan;
}

void ballu_state_temp_up(BalluState* state) {
    if(state && state->temp < BALLU_TEMP_MAX) state->temp++;
}

void ballu_state_temp_down(BalluState* state) {
    if(state && state->temp > BALLU_TEMP_MIN) state->temp--;
}

void ballu_state_toggle(BalluState* state, BalluToggle toggle) {
    if(!state || toggle >= BalluToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(ballu_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool ballu_state_toggle_active(const BalluState* state, BalluToggle toggle) {
    if(!state || toggle >= BalluToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool ballu_state_can_change_fan(const BalluState* state) {
    if(!state || state->mode == BalluModeOff) return false;
    return !ballu_ir_mode_locks_fan(state->mode);
}

bool ballu_state_can_change_temp(const BalluState* state) {
    if(!state || state->mode == BalluModeOff) return false;
    return !ballu_ir_mode_has_no_temp(state->mode);
}

BalluRequest ballu_state_request(const BalluState* state) {
    BalluRequest req = {BalluModeOff, BalluFanAuto, BALLU_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void ballu_state_format_current(const BalluState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    BalluRequest req = ballu_state_request(state);
    ballu_ir_format_state(&req, out, len);
}
