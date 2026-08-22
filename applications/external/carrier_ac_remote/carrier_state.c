#include "carrier_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define CARRIER_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} CarrierStateStorage;

static void to_storage(const CarrierState* s, CarrierStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(CarrierState* s, const CarrierStateStorage* i) {
    s->mode = (CarrierMode)i->mode;
    s->last_active_mode = (CarrierMode)i->last_active_mode;
    s->fan = (CarrierFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

CarrierState* carrier_state_alloc(void) {
    CarrierState* state = malloc(sizeof(CarrierState));
    furi_assert(state);
    carrier_state_reset(state);
    return state;
}

void carrier_state_free(CarrierState* state) {
    if(state) free(state);
}

void carrier_state_reset(CarrierState* state) {
    if(!state) return;
    state->mode = CARRIER_DEFAULT_MODE;
    state->last_active_mode = CARRIER_DEFAULT_MODE;
    state->fan = CARRIER_DEFAULT_FAN;
    state->temp = CARRIER_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = CARRIER_DEFAULT_SAVE_STATE;
}

bool carrier_state_load(CarrierState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, CARRIER_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        carrier_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, CARRIER_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == CARRIER_STATE_MAGIC && version == CARRIER_STATE_FILE_VERSION) {
            CarrierStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < CarrierModeCount && tmp.last_active_mode < CarrierModeCount &&
                   tmp.last_active_mode != CarrierModeOff && tmp.fan < CarrierFanCount &&
                   tmp.temp >= CARRIER_TEMP_MIN && tmp.temp <= CARRIER_TEMP_MAX &&
                   (carrier_ir_get_option_count() == 0 ||
                    tmp.option < carrier_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        carrier_state_reset(state);
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

    if(!success) carrier_state_reset(state);
    return success;
}

bool carrier_state_save(CarrierState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, CARRIER_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = CARRIER_STATE_MAGIC;
        uint8_t version = CARRIER_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            CarrierStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                CarrierState def;
                carrier_state_reset(&def);
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

void carrier_state_set_mode(CarrierState* state, CarrierMode mode) {
    if(!state || mode >= CarrierModeCount) return;
    state->mode = mode;
    if(mode != CarrierModeOff) state->last_active_mode = mode;
}

void carrier_state_set_fan(CarrierState* state, CarrierFan fan) {
    if(!state || fan >= CarrierFanCount) return;
    state->fan = fan;
}

void carrier_state_temp_up(CarrierState* state) {
    if(state && state->temp < CARRIER_TEMP_MAX) state->temp++;
}

void carrier_state_temp_down(CarrierState* state) {
    if(state && state->temp > CARRIER_TEMP_MIN) state->temp--;
}

void carrier_state_toggle(CarrierState* state, CarrierToggle toggle) {
    if(!state || toggle >= CarrierToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(carrier_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool carrier_state_toggle_active(const CarrierState* state, CarrierToggle toggle) {
    if(!state || toggle >= CarrierToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool carrier_state_can_change_fan(const CarrierState* state) {
    if(!state || state->mode == CarrierModeOff) return false;
    return !carrier_ir_mode_locks_fan(state->mode);
}

bool carrier_state_can_change_temp(const CarrierState* state) {
    if(!state || state->mode == CarrierModeOff) return false;
    return !carrier_ir_mode_has_no_temp(state->mode);
}

CarrierRequest carrier_state_request(const CarrierState* state) {
    CarrierRequest req = {CarrierModeOff, CarrierFanAuto, CARRIER_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void carrier_state_format_current(const CarrierState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    CarrierRequest req = carrier_state_request(state);
    carrier_ir_format_state(&req, out, len);
}
