#include "toshiba_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define TOSHIBA_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} ToshibaStateStorage;

static void to_storage(const ToshibaState* s, ToshibaStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(ToshibaState* s, const ToshibaStateStorage* i) {
    s->mode = (ToshibaMode)i->mode;
    s->last_active_mode = (ToshibaMode)i->last_active_mode;
    s->fan = (ToshibaFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

ToshibaState* toshiba_state_alloc(void) {
    ToshibaState* state = malloc(sizeof(ToshibaState));
    furi_assert(state);
    toshiba_state_reset(state);
    return state;
}

void toshiba_state_free(ToshibaState* state) {
    if(state) free(state);
}

void toshiba_state_reset(ToshibaState* state) {
    if(!state) return;
    state->mode = TOSHIBA_DEFAULT_MODE;
    state->last_active_mode = TOSHIBA_DEFAULT_MODE;
    state->fan = TOSHIBA_DEFAULT_FAN;
    state->temp = TOSHIBA_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = TOSHIBA_DEFAULT_SAVE_STATE;
}

bool toshiba_state_load(ToshibaState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, TOSHIBA_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        toshiba_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, TOSHIBA_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == TOSHIBA_STATE_MAGIC && version == TOSHIBA_STATE_FILE_VERSION) {
            ToshibaStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < ToshibaModeCount && tmp.last_active_mode < ToshibaModeCount &&
                   tmp.last_active_mode != ToshibaModeOff && tmp.fan < ToshibaFanCount &&
                   tmp.temp >= TOSHIBA_TEMP_MIN && tmp.temp <= TOSHIBA_TEMP_MAX &&
                   (toshiba_ir_get_option_count() == 0 ||
                    tmp.option < toshiba_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        toshiba_state_reset(state);
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

    if(!success) toshiba_state_reset(state);
    return success;
}

bool toshiba_state_save(ToshibaState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, TOSHIBA_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = TOSHIBA_STATE_MAGIC;
        uint8_t version = TOSHIBA_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            ToshibaStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                ToshibaState def;
                toshiba_state_reset(&def);
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

void toshiba_state_set_mode(ToshibaState* state, ToshibaMode mode) {
    if(!state || mode >= ToshibaModeCount) return;
    state->mode = mode;
    if(mode != ToshibaModeOff) state->last_active_mode = mode;
}

void toshiba_state_set_fan(ToshibaState* state, ToshibaFan fan) {
    if(!state || fan >= ToshibaFanCount) return;
    state->fan = fan;
}

void toshiba_state_temp_up(ToshibaState* state) {
    if(state && state->temp < TOSHIBA_TEMP_MAX) state->temp++;
}

void toshiba_state_temp_down(ToshibaState* state) {
    if(state && state->temp > TOSHIBA_TEMP_MIN) state->temp--;
}

void toshiba_state_toggle(ToshibaState* state, ToshibaToggle toggle) {
    if(!state || toggle >= ToshibaToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(toshiba_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool toshiba_state_toggle_active(const ToshibaState* state, ToshibaToggle toggle) {
    if(!state || toggle >= ToshibaToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool toshiba_state_can_change_fan(const ToshibaState* state) {
    if(!state || state->mode == ToshibaModeOff) return false;
    return !toshiba_ir_mode_locks_fan(state->mode);
}

bool toshiba_state_can_change_temp(const ToshibaState* state) {
    if(!state || state->mode == ToshibaModeOff) return false;
    return !toshiba_ir_mode_has_no_temp(state->mode);
}

ToshibaRequest toshiba_state_request(const ToshibaState* state) {
    ToshibaRequest req = {ToshibaModeOff, ToshibaFanAuto, TOSHIBA_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void toshiba_state_format_current(const ToshibaState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    ToshibaRequest req = toshiba_state_request(state);
    toshiba_ir_format_state(&req, out, len);
}
