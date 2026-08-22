#include "fujitsu_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define FUJITSU_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} FujitsuStateStorage;

static void to_storage(const FujitsuState* s, FujitsuStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(FujitsuState* s, const FujitsuStateStorage* i) {
    s->mode = (FujitsuMode)i->mode;
    s->last_active_mode = (FujitsuMode)i->last_active_mode;
    s->fan = (FujitsuFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

FujitsuState* fujitsu_state_alloc(void) {
    FujitsuState* state = malloc(sizeof(FujitsuState));
    furi_assert(state);
    fujitsu_state_reset(state);
    return state;
}

void fujitsu_state_free(FujitsuState* state) {
    if(state) free(state);
}

void fujitsu_state_reset(FujitsuState* state) {
    if(!state) return;
    state->mode = FUJITSU_DEFAULT_MODE;
    state->last_active_mode = FUJITSU_DEFAULT_MODE;
    state->fan = FUJITSU_DEFAULT_FAN;
    state->temp = FUJITSU_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = FUJITSU_DEFAULT_SAVE_STATE;
}

bool fujitsu_state_load(FujitsuState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, FUJITSU_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        fujitsu_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, FUJITSU_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == FUJITSU_STATE_MAGIC && version == FUJITSU_STATE_FILE_VERSION) {
            FujitsuStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < FujitsuModeCount && tmp.last_active_mode < FujitsuModeCount &&
                   tmp.last_active_mode != FujitsuModeOff && tmp.fan < FujitsuFanCount &&
                   tmp.temp >= FUJITSU_TEMP_MIN && tmp.temp <= FUJITSU_TEMP_MAX &&
                   (fujitsu_ir_get_option_count() == 0 ||
                    tmp.option < fujitsu_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        fujitsu_state_reset(state);
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

    if(!success) fujitsu_state_reset(state);
    return success;
}

bool fujitsu_state_save(FujitsuState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, FUJITSU_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = FUJITSU_STATE_MAGIC;
        uint8_t version = FUJITSU_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            FujitsuStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                FujitsuState def;
                fujitsu_state_reset(&def);
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

void fujitsu_state_set_mode(FujitsuState* state, FujitsuMode mode) {
    if(!state || mode >= FujitsuModeCount) return;
    state->mode = mode;
    if(mode != FujitsuModeOff) state->last_active_mode = mode;
}

void fujitsu_state_set_fan(FujitsuState* state, FujitsuFan fan) {
    if(!state || fan >= FujitsuFanCount) return;
    state->fan = fan;
}

void fujitsu_state_temp_up(FujitsuState* state) {
    if(state && state->temp < FUJITSU_TEMP_MAX) state->temp++;
}

void fujitsu_state_temp_down(FujitsuState* state) {
    if(state && state->temp > FUJITSU_TEMP_MIN) state->temp--;
}

void fujitsu_state_toggle(FujitsuState* state, FujitsuToggle toggle) {
    if(!state || toggle >= FujitsuToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(fujitsu_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool fujitsu_state_toggle_active(const FujitsuState* state, FujitsuToggle toggle) {
    if(!state || toggle >= FujitsuToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool fujitsu_state_can_change_fan(const FujitsuState* state) {
    if(!state || state->mode == FujitsuModeOff) return false;
    return !fujitsu_ir_mode_locks_fan(state->mode);
}

bool fujitsu_state_can_change_temp(const FujitsuState* state) {
    if(!state || state->mode == FujitsuModeOff) return false;
    return !fujitsu_ir_mode_has_no_temp(state->mode);
}

FujitsuRequest fujitsu_state_request(const FujitsuState* state) {
    FujitsuRequest req = {FujitsuModeOff, FujitsuFanAuto, FUJITSU_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void fujitsu_state_format_current(const FujitsuState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    FujitsuRequest req = fujitsu_state_request(state);
    fujitsu_ir_format_state(&req, out, len);
}
