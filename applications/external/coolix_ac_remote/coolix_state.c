#include "coolix_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

// Magic for saved struct validation
#define COOLIX_STATE_MAGIC 0x434C5853 // "CLXS"

// Number of vane positions the Direct button cycles through (display only)
#define COOLIX_DIRECT_STEPS 6

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint8_t swing;
    uint8_t turbo;
    uint8_t led;
    uint8_t sleep;
    uint8_t direct_step;
    uint8_t save_state;
} CoolixStateStorage;

static void coolix_state_to_storage(const CoolixState* state, CoolixStateStorage* out) {
    out->mode = state->mode;
    out->last_active_mode = state->last_active_mode;
    out->fan = state->fan;
    out->temp = state->temp;
    out->swing = state->swing ? 1 : 0;
    out->turbo = state->turbo ? 1 : 0;
    out->led = state->led ? 1 : 0;
    out->sleep = state->sleep ? 1 : 0;
    out->direct_step = state->direct_step;
    out->save_state = state->save_state ? 1 : 0;
}

static void coolix_state_from_storage(CoolixState* state, const CoolixStateStorage* in) {
    state->mode = (CoolixMode)in->mode;
    state->last_active_mode = (CoolixMode)in->last_active_mode;
    state->fan = (CoolixFan)in->fan;
    state->temp = in->temp;
    state->swing = in->swing != 0;
    state->turbo = in->turbo != 0;
    state->led = in->led != 0;
    state->sleep = in->sleep != 0;
    state->direct_step = in->direct_step;
    state->save_state = in->save_state != 0;
}

CoolixState* coolix_state_alloc(void) {
    CoolixState* state = malloc(sizeof(CoolixState));
    furi_assert(state);
    coolix_state_reset(state);
    return state;
}

void coolix_state_free(CoolixState* state) {
    if(state) {
        free(state);
    }
}

void coolix_state_reset(CoolixState* state) {
    if(!state) return;

    state->mode = COOLIX_DEFAULT_MODE;
    state->last_active_mode = COOLIX_DEFAULT_MODE;
    state->fan = COOLIX_DEFAULT_FAN;
    state->temp = COOLIX_DEFAULT_TEMP;

    state->swing = false;
    state->turbo = false;
    state->led = true; // LED usually on by default
    state->sleep = false;
    state->direct_step = 0;

    state->save_state = COOLIX_DEFAULT_SAVE_STATE;
}

bool coolix_state_load(CoolixState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Check if file exists
    if(!storage_file_exists(storage, COOLIX_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        coolix_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, COOLIX_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Read header
        uint32_t magic = 0;
        uint8_t version = 0;

        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version)) {
            if(magic == COOLIX_STATE_MAGIC && version == COOLIX_STATE_FILE_VERSION) {
                // Read state data
                CoolixStateStorage temp_state;
                if(storage_file_read(file, &temp_state, sizeof(temp_state)) ==
                   sizeof(temp_state)) {
                    // Validate loaded data
                    if(temp_state.mode < CoolixModeCount &&
                       temp_state.last_active_mode < CoolixModeCount &&
                       temp_state.last_active_mode != CoolixModeOff &&
                       temp_state.fan < CoolixFanCount && temp_state.temp >= COOLIX_TEMP_MIN &&
                       temp_state.temp <= COOLIX_TEMP_MAX &&
                       temp_state.direct_step < COOLIX_DIRECT_STEPS) {
                        // Check save_state setting
                        if(temp_state.save_state) {
                            // Copy all data
                            coolix_state_from_storage(state, &temp_state);
                            success = true;
                        } else {
                            // save_state was false - reset to defaults but keep save_state=false
                            coolix_state_reset(state);
                            state->save_state = false;
                            success = true;
                        }
                    }
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) {
        coolix_state_reset(state);
    }

    return success;
}

bool coolix_state_save(CoolixState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, COOLIX_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Write header
        uint32_t magic = COOLIX_STATE_MAGIC;
        uint8_t version = COOLIX_STATE_FILE_VERSION;

        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            CoolixStateStorage storage_state;
            if(state->save_state) {
                // Save full state
                coolix_state_to_storage(state, &storage_state);
            } else {
                // Save only save_state=false, rest as defaults
                CoolixState temp_state;
                coolix_state_reset(&temp_state);
                temp_state.save_state = false;
                coolix_state_to_storage(&temp_state, &storage_state);
            }
            success = storage_file_write(file, &storage_state, sizeof(storage_state)) ==
                      sizeof(storage_state);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return success;
}

void coolix_state_set_mode(CoolixState* state, CoolixMode mode) {
    if(!state) return;
    if(mode >= CoolixModeCount) return;
    state->mode = mode;
    if(state->mode != CoolixModeOff) {
        state->last_active_mode = state->mode;
    }
}

void coolix_state_set_fan(CoolixState* state, CoolixFan fan) {
    if(!state) return;
    if(fan >= CoolixFanCount) return;
    state->fan = fan;
}

void coolix_state_temp_up(CoolixState* state) {
    if(!state) return;
    if(state->temp < COOLIX_TEMP_MAX) {
        state->temp++;
    }
}

void coolix_state_temp_down(CoolixState* state) {
    if(!state) return;
    if(state->temp > COOLIX_TEMP_MIN) {
        state->temp--;
    }
}

void coolix_state_toggle(CoolixState* state, CoolixToggle toggle) {
    if(!state) return;

    switch(toggle) {
    case CoolixToggleSwing:
        state->swing = !state->swing;
        break;
    case CoolixToggleDirect:
        // The unit has no "set position N" command, each press steps the vane
        state->direct_step = (state->direct_step + 1) % COOLIX_DIRECT_STEPS;
        break;
    case CoolixToggleTurbo:
        state->turbo = !state->turbo;
        break;
    case CoolixToggleLed:
        state->led = !state->led;
        break;
    case CoolixToggleSleep:
        state->sleep = !state->sleep;
        break;
    default:
        break;
    }
}

bool coolix_state_can_change_fan(const CoolixState* state) {
    if(!state) return false;
    // Auto and Dry modes have fixed fan speed
    return state->mode != CoolixModeAuto && state->mode != CoolixModeDry &&
           state->mode != CoolixModeOff;
}

bool coolix_state_can_change_temp(const CoolixState* state) {
    if(!state) return false;
    // Fan-only mode has no temperature, Off mode also
    return state->mode != CoolixModeFan && state->mode != CoolixModeOff;
}

uint32_t coolix_state_current_code(const CoolixState* state) {
    if(!state) return 0;
    return coolix_ir_build_state(state->mode, state->fan, state->temp);
}
