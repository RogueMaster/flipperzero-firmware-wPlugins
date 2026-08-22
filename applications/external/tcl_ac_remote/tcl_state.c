#include "tcl_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define TCL_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} TclStateStorage;

static void to_storage(const TclState* s, TclStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(TclState* s, const TclStateStorage* i) {
    s->mode = (TclMode)i->mode;
    s->last_active_mode = (TclMode)i->last_active_mode;
    s->fan = (TclFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

TclState* tcl_state_alloc(void) {
    TclState* state = malloc(sizeof(TclState));
    furi_assert(state);
    tcl_state_reset(state);
    return state;
}

void tcl_state_free(TclState* state) {
    if(state) free(state);
}

void tcl_state_reset(TclState* state) {
    if(!state) return;
    state->mode = TCL_DEFAULT_MODE;
    state->last_active_mode = TCL_DEFAULT_MODE;
    state->fan = TCL_DEFAULT_FAN;
    state->temp = TCL_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = TCL_DEFAULT_SAVE_STATE;
}

bool tcl_state_load(TclState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, TCL_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        tcl_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, TCL_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == TCL_STATE_MAGIC && version == TCL_STATE_FILE_VERSION) {
            TclStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < TclModeCount && tmp.last_active_mode < TclModeCount &&
                   tmp.last_active_mode != TclModeOff && tmp.fan < TclFanCount &&
                   tmp.temp >= TCL_TEMP_MIN && tmp.temp <= TCL_TEMP_MAX &&
                   (tcl_ir_get_option_count() == 0 || tmp.option < tcl_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        tcl_state_reset(state);
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

    if(!success) tcl_state_reset(state);
    return success;
}

bool tcl_state_save(TclState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, TCL_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = TCL_STATE_MAGIC;
        uint8_t version = TCL_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            TclStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                TclState def;
                tcl_state_reset(&def);
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

void tcl_state_set_mode(TclState* state, TclMode mode) {
    if(!state || mode >= TclModeCount) return;
    state->mode = mode;
    if(mode != TclModeOff) state->last_active_mode = mode;
}

void tcl_state_set_fan(TclState* state, TclFan fan) {
    if(!state || fan >= TclFanCount) return;
    state->fan = fan;
}

void tcl_state_temp_up(TclState* state) {
    if(state && state->temp < TCL_TEMP_MAX) state->temp++;
}

void tcl_state_temp_down(TclState* state) {
    if(state && state->temp > TCL_TEMP_MIN) state->temp--;
}

void tcl_state_toggle(TclState* state, TclToggle toggle) {
    if(!state || toggle >= TclToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(tcl_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool tcl_state_toggle_active(const TclState* state, TclToggle toggle) {
    if(!state || toggle >= TclToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool tcl_state_can_change_fan(const TclState* state) {
    if(!state || state->mode == TclModeOff) return false;
    return !tcl_ir_mode_locks_fan(state->mode);
}

bool tcl_state_can_change_temp(const TclState* state) {
    if(!state || state->mode == TclModeOff) return false;
    return !tcl_ir_mode_has_no_temp(state->mode);
}

TclRequest tcl_state_request(const TclState* state) {
    TclRequest req = {TclModeOff, TclFanAuto, TCL_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void tcl_state_format_current(const TclState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    TclRequest req = tcl_state_request(state);
    tcl_ir_format_state(&req, out, len);
}
