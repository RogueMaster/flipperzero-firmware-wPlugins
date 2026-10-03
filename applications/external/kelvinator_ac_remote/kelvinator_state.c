#include "kelvinator_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define KELVINATOR_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} KelvinatorStateStorage;

static void to_storage(const KelvinatorState* s, KelvinatorStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(KelvinatorState* s, const KelvinatorStateStorage* i) {
    s->mode = (KelvinatorMode)i->mode;
    s->last_active_mode = (KelvinatorMode)i->last_active_mode;
    s->fan = (KelvinatorFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

KelvinatorState* kelvinator_state_alloc(void) {
    KelvinatorState* state = malloc(sizeof(KelvinatorState));
    furi_assert(state);
    kelvinator_state_reset(state);
    return state;
}

void kelvinator_state_free(KelvinatorState* state) {
    if(state) free(state);
}

void kelvinator_state_reset(KelvinatorState* state) {
    if(!state) return;
    state->mode = KELVINATOR_DEFAULT_MODE;
    state->last_active_mode = KELVINATOR_DEFAULT_MODE;
    state->fan = KELVINATOR_DEFAULT_FAN;
    state->temp = KELVINATOR_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = KELVINATOR_DEFAULT_SAVE_STATE;
}

bool kelvinator_state_load(KelvinatorState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, KELVINATOR_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        kelvinator_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, KELVINATOR_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == KELVINATOR_STATE_MAGIC && version == KELVINATOR_STATE_FILE_VERSION) {
            KelvinatorStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < KelvinatorModeCount && tmp.last_active_mode < KelvinatorModeCount &&
                   tmp.last_active_mode != KelvinatorModeOff && tmp.fan < KelvinatorFanCount &&
                   tmp.temp >= KELVINATOR_TEMP_MIN && tmp.temp <= KELVINATOR_TEMP_MAX &&
                   (kelvinator_ir_get_option_count() == 0 ||
                    tmp.option < kelvinator_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        kelvinator_state_reset(state);
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

    if(!success) kelvinator_state_reset(state);
    return success;
}

bool kelvinator_state_save(KelvinatorState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, KELVINATOR_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = KELVINATOR_STATE_MAGIC;
        uint8_t version = KELVINATOR_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            KelvinatorStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                KelvinatorState def;
                kelvinator_state_reset(&def);
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

void kelvinator_state_set_mode(KelvinatorState* state, KelvinatorMode mode) {
    if(!state || mode >= KelvinatorModeCount) return;
    state->mode = mode;
    if(mode != KelvinatorModeOff) state->last_active_mode = mode;
}

void kelvinator_state_set_fan(KelvinatorState* state, KelvinatorFan fan) {
    if(!state || fan >= KelvinatorFanCount) return;
    state->fan = fan;
}

void kelvinator_state_temp_up(KelvinatorState* state) {
    if(state && state->temp < KELVINATOR_TEMP_MAX) state->temp++;
}

void kelvinator_state_temp_down(KelvinatorState* state) {
    if(state && state->temp > KELVINATOR_TEMP_MIN) state->temp--;
}

void kelvinator_state_toggle(KelvinatorState* state, KelvinatorToggle toggle) {
    if(!state || toggle >= KelvinatorToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(kelvinator_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool kelvinator_state_toggle_active(const KelvinatorState* state, KelvinatorToggle toggle) {
    if(!state || toggle >= KelvinatorToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool kelvinator_state_can_change_fan(const KelvinatorState* state) {
    if(!state || state->mode == KelvinatorModeOff) return false;
    return !kelvinator_ir_mode_locks_fan(state->mode);
}

bool kelvinator_state_can_change_temp(const KelvinatorState* state) {
    if(!state || state->mode == KelvinatorModeOff) return false;
    return !kelvinator_ir_mode_has_no_temp(state->mode);
}

KelvinatorRequest kelvinator_state_request(const KelvinatorState* state) {
    KelvinatorRequest req = {KelvinatorModeOff, KelvinatorFanAuto, KELVINATOR_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void kelvinator_state_format_current(const KelvinatorState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    KelvinatorRequest req = kelvinator_state_request(state);
    kelvinator_ir_format_state(&req, out, len);
}
