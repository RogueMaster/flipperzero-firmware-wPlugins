#include "mitsubishi_heavy_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define MITSUBISHI_HEAVY_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} MitsubishiHeavyStateStorage;

static void to_storage(const MitsubishiHeavyState* s, MitsubishiHeavyStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(MitsubishiHeavyState* s, const MitsubishiHeavyStateStorage* i) {
    s->mode = (MitsubishiHeavyMode)i->mode;
    s->last_active_mode = (MitsubishiHeavyMode)i->last_active_mode;
    s->fan = (MitsubishiHeavyFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

MitsubishiHeavyState* mitsubishi_heavy_state_alloc(void) {
    MitsubishiHeavyState* state = malloc(sizeof(MitsubishiHeavyState));
    furi_assert(state);
    mitsubishi_heavy_state_reset(state);
    return state;
}

void mitsubishi_heavy_state_free(MitsubishiHeavyState* state) {
    if(state) free(state);
}

void mitsubishi_heavy_state_reset(MitsubishiHeavyState* state) {
    if(!state) return;
    state->mode = MITSUBISHI_HEAVY_DEFAULT_MODE;
    state->last_active_mode = MITSUBISHI_HEAVY_DEFAULT_MODE;
    state->fan = MITSUBISHI_HEAVY_DEFAULT_FAN;
    state->temp = MITSUBISHI_HEAVY_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = MITSUBISHI_HEAVY_DEFAULT_SAVE_STATE;
}

bool mitsubishi_heavy_state_load(MitsubishiHeavyState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, MITSUBISHI_HEAVY_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        mitsubishi_heavy_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, MITSUBISHI_HEAVY_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == MITSUBISHI_HEAVY_STATE_MAGIC &&
           version == MITSUBISHI_HEAVY_STATE_FILE_VERSION) {
            MitsubishiHeavyStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < MitsubishiHeavyModeCount &&
                   tmp.last_active_mode < MitsubishiHeavyModeCount &&
                   tmp.last_active_mode != MitsubishiHeavyModeOff &&
                   tmp.fan < MitsubishiHeavyFanCount && tmp.temp >= MITSUBISHI_HEAVY_TEMP_MIN &&
                   tmp.temp <= MITSUBISHI_HEAVY_TEMP_MAX &&
                   (mitsubishi_heavy_ir_get_option_count() == 0 ||
                    tmp.option < mitsubishi_heavy_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        mitsubishi_heavy_state_reset(state);
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

    if(!success) mitsubishi_heavy_state_reset(state);
    return success;
}

bool mitsubishi_heavy_state_save(MitsubishiHeavyState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, MITSUBISHI_HEAVY_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = MITSUBISHI_HEAVY_STATE_MAGIC;
        uint8_t version = MITSUBISHI_HEAVY_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            MitsubishiHeavyStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                MitsubishiHeavyState def;
                mitsubishi_heavy_state_reset(&def);
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

void mitsubishi_heavy_state_set_mode(MitsubishiHeavyState* state, MitsubishiHeavyMode mode) {
    if(!state || mode >= MitsubishiHeavyModeCount) return;
    state->mode = mode;
    if(mode != MitsubishiHeavyModeOff) state->last_active_mode = mode;
}

void mitsubishi_heavy_state_set_fan(MitsubishiHeavyState* state, MitsubishiHeavyFan fan) {
    if(!state || fan >= MitsubishiHeavyFanCount) return;
    state->fan = fan;
}

void mitsubishi_heavy_state_temp_up(MitsubishiHeavyState* state) {
    if(state && state->temp < MITSUBISHI_HEAVY_TEMP_MAX) state->temp++;
}

void mitsubishi_heavy_state_temp_down(MitsubishiHeavyState* state) {
    if(state && state->temp > MITSUBISHI_HEAVY_TEMP_MIN) state->temp--;
}

void mitsubishi_heavy_state_toggle(MitsubishiHeavyState* state, MitsubishiHeavyToggle toggle) {
    if(!state || toggle >= MitsubishiHeavyToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(mitsubishi_heavy_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool mitsubishi_heavy_state_toggle_active(
    const MitsubishiHeavyState* state,
    MitsubishiHeavyToggle toggle) {
    if(!state || toggle >= MitsubishiHeavyToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool mitsubishi_heavy_state_can_change_fan(const MitsubishiHeavyState* state) {
    if(!state || state->mode == MitsubishiHeavyModeOff) return false;
    return !mitsubishi_heavy_ir_mode_locks_fan(state->mode);
}

bool mitsubishi_heavy_state_can_change_temp(const MitsubishiHeavyState* state) {
    if(!state || state->mode == MitsubishiHeavyModeOff) return false;
    return !mitsubishi_heavy_ir_mode_has_no_temp(state->mode);
}

MitsubishiHeavyRequest mitsubishi_heavy_state_request(const MitsubishiHeavyState* state) {
    MitsubishiHeavyRequest req = {
        MitsubishiHeavyModeOff, MitsubishiHeavyFanAuto, MITSUBISHI_HEAVY_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void mitsubishi_heavy_state_format_current(
    const MitsubishiHeavyState* state,
    char* out,
    size_t len) {
    if(!state || !out || !len) return;
    MitsubishiHeavyRequest req = mitsubishi_heavy_state_request(state);
    mitsubishi_heavy_ir_format_state(&req, out, len);
}
