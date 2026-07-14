#include "htw_state.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>

// Magic for saved struct validation
#define HTW_STATE_MAGIC 0x48545753 // "HTWS"

typedef struct {
    uint8_t mode;
    uint8_t last_active_mode;
    uint8_t fan;
    uint8_t temp;
    uint8_t swing;
    uint8_t turbo;
    uint8_t fresh;
    uint8_t led;
    uint8_t clean;
    uint8_t timer_on_step;
    uint8_t timer_off_step;
    uint8_t save_state;
} HtwStateStorage;

static void htw_state_to_storage(const HtwState* state, HtwStateStorage* out) {
    out->mode = state->mode;
    out->last_active_mode = state->last_active_mode;
    out->fan = state->fan;
    out->temp = state->temp;
    out->swing = state->swing ? 1 : 0;
    out->turbo = state->turbo ? 1 : 0;
    out->fresh = state->fresh ? 1 : 0;
    out->led = state->led ? 1 : 0;
    out->clean = state->clean ? 1 : 0;
    out->timer_on_step = state->timer_on_step;
    out->timer_off_step = state->timer_off_step;
    out->save_state = state->save_state ? 1 : 0;
}

static void htw_state_from_storage(HtwState* state, const HtwStateStorage* in) {
    state->mode = (HtwMode)in->mode;
    state->last_active_mode = (HtwMode)in->last_active_mode;
    state->fan = (HtwFan)in->fan;
    state->temp = in->temp;
    state->swing = in->swing != 0;
    state->turbo = in->turbo != 0;
    state->fresh = in->fresh != 0;
    state->led = in->led != 0;
    state->clean = in->clean != 0;
    state->timer_on_step = in->timer_on_step;
    state->timer_off_step = in->timer_off_step;
    state->save_state = in->save_state != 0;
}

HtwState* htw_state_alloc(void) {
    HtwState* state = malloc(sizeof(HtwState));
    furi_assert(state);
    htw_state_reset(state);
    return state;
}

void htw_state_free(HtwState* state) {
    if(state) {
        free(state);
    }
}

void htw_state_reset(HtwState* state) {
    if(!state) return;

    state->mode = HTW_DEFAULT_MODE;
    state->last_active_mode = HTW_DEFAULT_MODE;
    state->fan = HTW_DEFAULT_FAN;
    state->temp = HTW_DEFAULT_TEMP;

    state->swing = false;
    state->turbo = false;
    state->fresh = false;
    state->led = true; // LED usually on by default
    state->clean = false;

    state->timer_on_step = 0;
    state->timer_off_step = 0;

    state->save_state = HTW_DEFAULT_SAVE_STATE;
}

bool htw_state_load(HtwState* state) {
    if(!state) return false;

    // First, try to load the file
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Check if file exists
    if(!storage_file_exists(storage, HTW_STATE_FILE_PATH)) {
        furi_record_close(RECORD_STORAGE);
        htw_state_reset(state);
        return false;
    }

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, HTW_STATE_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Read header
        uint32_t magic = 0;
        uint8_t version = 0;

        if(storage_file_read(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_read(file, &version, sizeof(version)) == sizeof(version)) {
            if(magic == HTW_STATE_MAGIC && version == HTW_STATE_FILE_VERSION) {
                // Read state data
                HtwStateStorage temp_state;
                if(storage_file_read(file, &temp_state, sizeof(temp_state)) ==
                   sizeof(temp_state)) {
                    // Validate loaded data
                    if(temp_state.mode < HtwModeCount &&
                       temp_state.last_active_mode < HtwModeCount &&
                       temp_state.fan < HtwFanCount && temp_state.temp >= HTW_TEMP_MIN &&
                       temp_state.temp <= HTW_TEMP_MAX &&
                       temp_state.timer_on_step <= HTW_TIMER_STEPS_COUNT &&
                       temp_state.timer_off_step <= HTW_TIMER_STEPS_COUNT) {
                        // Check save_state setting
                        if(temp_state.save_state) {
                            // Copy all data
                            htw_state_from_storage(state, &temp_state);
                            success = true;
                        } else {
                            // save_state was false - reset to defaults but keep save_state=false
                            htw_state_reset(state);
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
        htw_state_reset(state);
    }

    return success;
}

bool htw_state_save(HtwState* state) {
    if(!state) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure directory exists
    storage_simply_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(file, HTW_STATE_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Write header
        uint32_t magic = HTW_STATE_MAGIC;
        uint8_t version = HTW_STATE_FILE_VERSION;

        if(storage_file_write(file, &magic, sizeof(magic)) == sizeof(magic) &&
           storage_file_write(file, &version, sizeof(version)) == sizeof(version)) {
            HtwStateStorage storage_state;
            if(state->save_state) {
                // Save full state
                htw_state_to_storage(state, &storage_state);
            } else {
                // Save only save_state=false, rest as defaults
                HtwState temp_state;
                htw_state_reset(&temp_state);
                temp_state.save_state = false;
                htw_state_to_storage(&temp_state, &storage_state);
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

void htw_state_mode_next(HtwState* state) {
    if(!state) return;
    state->mode = (state->mode + 1) % HtwModeCount;
    if(state->mode != HtwModeOff) {
        state->last_active_mode = state->mode;
    }
}

void htw_state_mode_prev(HtwState* state) {
    if(!state) return;
    if(state->mode == 0) {
        state->mode = HtwModeCount - 1;
    } else {
        state->mode--;
    }
    if(state->mode != HtwModeOff) {
        state->last_active_mode = state->mode;
    }
}

void htw_state_fan_next(HtwState* state) {
    if(!state) return;
    state->fan = (state->fan + 1) % HtwFanCount;
}

void htw_state_fan_prev(HtwState* state) {
    if(!state) return;
    if(state->fan == 0) {
        state->fan = HtwFanCount - 1;
    } else {
        state->fan--;
    }
}

void htw_state_temp_up(HtwState* state) {
    if(!state) return;
    if(state->temp < HTW_TEMP_MAX) {
        state->temp++;
    }
}

void htw_state_temp_down(HtwState* state) {
    if(!state) return;
    if(state->temp > HTW_TEMP_MIN) {
        state->temp--;
    }
}

void htw_state_toggle(HtwState* state, HtwToggle toggle) {
    if(!state) return;

    switch(toggle) {
    case HtwToggleSwing:
        state->swing = !state->swing;
        break;
    case HtwToggleLed:
        state->led = !state->led;
        break;
    case HtwToggleTurbo:
        state->turbo = !state->turbo;
        break;
    case HtwToggleFresh:
        state->fresh = !state->fresh;
        break;
    case HtwToggleClean:
        state->clean = !state->clean;
        break;
    default:
        break;
    }
}

void htw_state_timer_on_cycle(HtwState* state, bool forward) {
    if(!state) return;

    if(forward) {
        if(state->timer_on_step < HTW_TIMER_STEPS_COUNT) {
            state->timer_on_step++;
        } else {
            state->timer_on_step = 0; // Wrap to off
        }
    } else {
        if(state->timer_on_step > 0) {
            state->timer_on_step--;
        } else {
            state->timer_on_step = HTW_TIMER_STEPS_COUNT; // Wrap to max
        }
    }
}

void htw_state_timer_off_cycle(HtwState* state, bool forward) {
    if(!state) return;

    if(forward) {
        if(state->timer_off_step < HTW_TIMER_STEPS_COUNT) {
            state->timer_off_step++;
        } else {
            state->timer_off_step = 0;
        }
    } else {
        if(state->timer_off_step > 0) {
            state->timer_off_step--;
        } else {
            state->timer_off_step = HTW_TIMER_STEPS_COUNT;
        }
    }
}

bool htw_state_can_change_fan(const HtwState* state) {
    if(!state) return false;
    // Auto and Dry modes have fixed fan speed
    return state->mode != HtwModeAuto && state->mode != HtwModeDry && state->mode != HtwModeOff;
}

bool htw_state_can_change_temp(const HtwState* state) {
    if(!state) return false;
    // Fan-only mode has no temperature, Off mode also
    return state->mode != HtwModeFan && state->mode != HtwModeOff;
}
