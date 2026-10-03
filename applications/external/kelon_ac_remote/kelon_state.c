#include "kelon_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

#define KELON_STATE_MAGIC 0x50525453 // "PRTS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint32_t toggle_bits;
    uint8_t option;
    uint8_t save_state;
} KelonStateStorage;

static void to_storage(const KelonState* s, KelonStateStorage* o) {
    o->mode = s->mode;
    o->last_active_mode = s->last_active_mode;
    o->fan = s->fan;
    o->temp = s->temp;
    o->toggle_bits = s->toggle_bits;
    o->option = s->option;
    o->save_state = s->save_state ? 1 : 0;
}

static void from_storage(KelonState* s, const KelonStateStorage* i) {
    s->mode = (KelonMode)i->mode;
    s->last_active_mode = (KelonMode)i->last_active_mode;
    s->fan = (KelonFan)i->fan;
    s->temp = i->temp;
    s->toggle_bits = i->toggle_bits;
    s->option = i->option;
    s->save_state = i->save_state != 0;
}

KelonState* kelon_state_alloc(void) {
    KelonState* state = malloc(sizeof(KelonState));
    furi_assert(state);
    kelon_state_reset(state);
    return state;
}

void kelon_state_free(KelonState* state) {
    if(state) free(state);
}

void kelon_state_reset(KelonState* state) {
    if(!state) return;
    state->mode = KELON_DEFAULT_MODE;
    state->last_active_mode = KELON_DEFAULT_MODE;
    state->fan = KELON_DEFAULT_FAN;
    state->temp = KELON_DEFAULT_TEMP;
    state->toggle_bits = 0;
    state->option = 0;
    state->save_state = KELON_DEFAULT_SAVE_STATE;
}

bool kelon_state_load(KelonState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_file_exists(storage, KELON_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        kelon_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, KELON_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t magic = 0;
        uint8_t version = 0;
        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version) &&
           magic == KELON_STATE_MAGIC && version == KELON_STATE_FILE_VERSION) {
            KelonStateStorage tmp;
            if(storage_file_read(file, &tmp, sizeof(tmp)) == sizeof(tmp)) {
                if(tmp.mode < KelonModeCount && tmp.last_active_mode < KelonModeCount &&
                   tmp.last_active_mode != KelonModeOff && tmp.fan < KelonFanCount &&
                   tmp.temp >= KELON_TEMP_MIN && tmp.temp <= KELON_TEMP_MAX &&
                   (kelon_ir_get_option_count() == 0 ||
                    tmp.option < kelon_ir_get_option_count())) {
                    if(tmp.save_state) {
                        from_storage(state, &tmp);
                    } else {
                        kelon_state_reset(state);
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

    if(!success) kelon_state_reset(state);
    return success;
}

bool kelon_state_save(KelonState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, KELON_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint32_t magic = KELON_STATE_MAGIC;
        uint8_t version = KELON_STATE_FILE_VERSION;
        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            KelonStateStorage out;
            if(state->save_state) {
                to_storage(state, &out);
            } else {
                KelonState def;
                kelon_state_reset(&def);
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

void kelon_state_set_mode(KelonState* state, KelonMode mode) {
    if(!state || mode >= KelonModeCount) return;
    state->mode = mode;
    if(mode != KelonModeOff) state->last_active_mode = mode;
}

void kelon_state_set_fan(KelonState* state, KelonFan fan) {
    if(!state || fan >= KelonFanCount) return;
    state->fan = fan;
}

void kelon_state_temp_up(KelonState* state) {
    if(state && state->temp < KELON_TEMP_MAX) state->temp++;
}

void kelon_state_temp_down(KelonState* state) {
    if(state && state->temp > KELON_TEMP_MIN) state->temp--;
}

void kelon_state_toggle(KelonState* state, KelonToggle toggle) {
    if(!state || toggle >= KelonToggleCount) return;
    // Momentary buttons (vane step, "next position") have no on/off state to
    // show, so they never light the indicator dot.
    if(kelon_ir_toggle_is_momentary(toggle)) return;
    state->toggle_bits ^= (1UL << toggle);
}

bool kelon_state_toggle_active(const KelonState* state, KelonToggle toggle) {
    if(!state || toggle >= KelonToggleCount) return false;
    return (state->toggle_bits >> toggle) & 1UL;
}

bool kelon_state_can_change_fan(const KelonState* state) {
    if(!state || state->mode == KelonModeOff) return false;
    return !kelon_ir_mode_locks_fan(state->mode);
}

bool kelon_state_can_change_temp(const KelonState* state) {
    if(!state || state->mode == KelonModeOff) return false;
    return !kelon_ir_mode_has_no_temp(state->mode);
}

KelonRequest kelon_state_request(const KelonState* state) {
    KelonRequest req = {KelonModeOff, KelonFanAuto, KELON_DEFAULT_TEMP, 0, 0};
    if(!state) return req;
    req.mode = state->mode;
    req.fan = state->fan;
    req.temp = state->temp;
    req.toggle_bits = state->toggle_bits;
    req.option = state->option;
    return req;
}

void kelon_state_format_current(const KelonState* state, char* out, size_t len) {
    if(!state || !out || !len) return;
    KelonRequest req = kelon_state_request(state);
    kelon_ir_format_state(&req, out, len);
}
