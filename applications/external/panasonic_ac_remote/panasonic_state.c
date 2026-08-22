#include "panasonic_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define PANASONIC_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} PanasonicStateStorage;

static void to_storage(const PanasonicState* s, PanasonicStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(PanasonicState* s, const PanasonicStateStorage* i) {
    s->mode = (PanasonicMode)i->mode;
    s->last_active_mode = (PanasonicMode)i->last_active_mode;
    s->fan = (PanasonicFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

PanasonicState* panasonic_state_alloc(void) {
    PanasonicState* state = malloc(sizeof(PanasonicState));
    furi_assert(state);
    panasonic_state_reset(state);
    return state;
}

void panasonic_state_free(PanasonicState* state) {
    if(state) free(state);
}

void panasonic_state_reset(PanasonicState* state) {
    if(!state) return;
    state->mode = PANASONIC_DEFAULT_MODE;
    state->last_active_mode = PANASONIC_DEFAULT_MODE;
    state->fan = PANASONIC_DEFAULT_FAN;
    state->temp = PANASONIC_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = PANASONIC_DEFAULT_SAVE_STATE;
}

bool panasonic_state_load(PanasonicState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, PANASONIC_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        panasonic_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, PANASONIC_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == PANASONIC_STATE_MAGIC && version == PANASONIC_STATE_FILE_VERSION) {
            PanasonicStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < PanasonicModeCount && tmp.last_active_mode < PanasonicModeCount &&
                   tmp.last_active_mode != PanasonicModeOff && tmp.fan < PanasonicFanCount &&
                   tmp.temp >= PANASONIC_TEMP_MIN && tmp.temp <= PANASONIC_TEMP_MAX &&
                   (panasonic_ir_get_option_count() == 0 ||
                    tmp.option < panasonic_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        panasonic_state_reset(state);
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

    if(!success) panasonic_state_reset(state);
    return success;
}

bool panasonic_state_save(PanasonicState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, PANASONIC_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = PANASONIC_STATE_MAGIC;
        uint8_t version = PANASONIC_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            PanasonicStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                PanasonicState def;
                panasonic_state_reset(&def);
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

void panasonic_state_set_mode(PanasonicState* state, PanasonicMode mode) {
    if(!state || mode >= PanasonicModeCount) return;
    state->mode = mode;
    if(mode != PanasonicModeOff) state->last_active_mode = mode;
}

void panasonic_state_set_fan(PanasonicState* state, PanasonicFan fan) {
    if(!state || fan >= PanasonicFanCount) return;
    state->fan = fan;
}

void panasonic_state_temp_up(PanasonicState* state) {
    if(state && state->temp < PANASONIC_TEMP_MAX) state->temp++;
}

void panasonic_state_temp_down(PanasonicState* state) {
    if(state && state->temp > PANASONIC_TEMP_MIN) state->temp--;
}

void panasonic_state_toggle(PanasonicState* state, PanasonicToggle toggle) {
    if(!state || toggle >= PanasonicToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(panasonic_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool panasonic_state_toggle_active(const PanasonicState* state, PanasonicToggle toggle) {
    if(!state || toggle >= PanasonicToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool panasonic_state_can_change_fan(const PanasonicState* state) {
    if(!state || state->mode == PanasonicModeOff) return false;
    return !panasonic_ir_mode_locks_fan(state->mode);
}

bool panasonic_state_can_change_temp(const PanasonicState* state) {
    if(!state || state->mode == PanasonicModeOff) return false;
    return !panasonic_ir_mode_has_no_temp(state->mode);
}

PanasonicRequest panasonic_state_request(const PanasonicState* state) {
    PanasonicRequest req = {PanasonicModeOff, PanasonicFanAuto, PANASONIC_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void panasonic_state_format_current(const PanasonicState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    PanasonicRequest req = panasonic_state_request(state);
    panasonic_ir_format_state(&req, out, len);
}
