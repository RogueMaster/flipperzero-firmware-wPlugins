#include "samsung_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define SAMSUNG_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} SamsungStateStorage;

static void to_storage(const SamsungState* s, SamsungStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(SamsungState* s, const SamsungStateStorage* i) {
    s->mode = (SamsungMode)i->mode;
    s->last_active_mode = (SamsungMode)i->last_active_mode;
    s->fan = (SamsungFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

SamsungState* samsung_state_alloc(void) {
    SamsungState* state = malloc(sizeof(SamsungState));
    furi_assert(state);
    samsung_state_reset(state);
    return state;
}

void samsung_state_free(SamsungState* state) {
    if(state) free(state);
}

void samsung_state_reset(SamsungState* state) {
    if(!state) return;
    state->mode = SAMSUNG_DEFAULT_MODE;
    state->last_active_mode = SAMSUNG_DEFAULT_MODE;
    state->fan = SAMSUNG_DEFAULT_FAN;
    state->temp = SAMSUNG_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = SAMSUNG_DEFAULT_SAVE_STATE;
}

bool samsung_state_load(SamsungState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, SAMSUNG_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        samsung_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, SAMSUNG_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == SAMSUNG_STATE_MAGIC && version == SAMSUNG_STATE_FILE_VERSION) {
            SamsungStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < SamsungModeCount && tmp.last_active_mode < SamsungModeCount &&
                   tmp.last_active_mode != SamsungModeOff && tmp.fan < SamsungFanCount &&
                   tmp.temp >= SAMSUNG_TEMP_MIN && tmp.temp <= SAMSUNG_TEMP_MAX &&
                   (samsung_ir_get_option_count() == 0 ||
                    tmp.option < samsung_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        samsung_state_reset(state);
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

    if(!success) samsung_state_reset(state);
    return success;
}

bool samsung_state_save(SamsungState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, SAMSUNG_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = SAMSUNG_STATE_MAGIC;
        uint8_t version = SAMSUNG_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            SamsungStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                SamsungState def;
                samsung_state_reset(&def);
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

void samsung_state_set_mode(SamsungState* state, SamsungMode mode) {
    if(!state || mode >= SamsungModeCount) return;
    state->mode = mode;
    if(mode != SamsungModeOff) state->last_active_mode = mode;
}

void samsung_state_set_fan(SamsungState* state, SamsungFan fan) {
    if(!state || fan >= SamsungFanCount) return;
    state->fan = fan;
}

void samsung_state_temp_up(SamsungState* state) {
    if(state && state->temp < SAMSUNG_TEMP_MAX) state->temp++;
}

void samsung_state_temp_down(SamsungState* state) {
    if(state && state->temp > SAMSUNG_TEMP_MIN) state->temp--;
}

void samsung_state_toggle(SamsungState* state, SamsungToggle toggle) {
    if(!state || toggle >= SamsungToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(samsung_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool samsung_state_toggle_active(const SamsungState* state, SamsungToggle toggle) {
    if(!state || toggle >= SamsungToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool samsung_state_can_change_fan(const SamsungState* state) {
    if(!state || state->mode == SamsungModeOff) return false;
    return !samsung_ir_mode_locks_fan(state->mode);
}

bool samsung_state_can_change_temp(const SamsungState* state) {
    if(!state || state->mode == SamsungModeOff) return false;
    return !samsung_ir_mode_has_no_temp(state->mode);
}

SamsungRequest samsung_state_request(const SamsungState* state) {
    SamsungRequest req = {SamsungModeOff, SamsungFanAuto, SAMSUNG_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void samsung_state_format_current(const SamsungState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    SamsungRequest req = samsung_state_request(state);
    samsung_ir_format_state(&req, out, len);
}
