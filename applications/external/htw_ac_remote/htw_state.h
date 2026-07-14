#pragma once

#include "htw_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Storage path for state file
#define HTW_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
#define HTW_STATE_FILE_VERSION 1

// Default values
#define HTW_DEFAULT_MODE       HtwModeCool
#define HTW_DEFAULT_FAN        HtwFanAuto
#define HTW_DEFAULT_TEMP       24
#define HTW_DEFAULT_SAVE_STATE true

/**
 * Application state structure
 */
typedef struct {
    // Current mode (including Off)
    HtwMode mode;

    // Last active mode (not Off) - used for Timer OFF when AC is off
    HtwMode last_active_mode;

    // Fan speed
    HtwFan fan;

    // Temperature (17-30)
    uint8_t temp;

    // Toggle states (virtual - no feedback from AC)
    bool swing;
    bool turbo;
    bool fresh;
    bool led;
    bool clean;

    // Timer steps (0 = disabled, 1-34 = timer value)
    uint8_t timer_on_step;
    uint8_t timer_off_step;

    // Settings
    bool save_state; // Whether to persist state across app restarts
} HtwState;

/**
 * Allocate and initialize state with defaults
 * @return Allocated state structure
 */
HtwState* htw_state_alloc(void);

/**
 * Free state structure
 * @param state State to free
 */
void htw_state_free(HtwState* state);

/**
 * Reset state to defaults
 * @param state State to reset
 */
void htw_state_reset(HtwState* state);

/**
 * Load state from storage
 * If loading fails or save_state is false, resets to defaults
 *
 * @param state State structure to load into
 * @return true if successfully loaded, false if reset to defaults
 */
bool htw_state_load(HtwState* state);

/**
 * Save state to storage
 * If save_state is false, only saves the save_state=false flag
 *
 * @param state State structure to save
 * @return true on success
 */
bool htw_state_save(HtwState* state);

/**
 * Cycle mode to next value
 * Order: Off -> Cool -> Auto -> Dry -> Heat -> Fan -> Off
 *
 * @param state State to modify
 */
void htw_state_mode_next(HtwState* state);

/**
 * Cycle mode to previous value
 * @param state State to modify
 */
void htw_state_mode_prev(HtwState* state);

/**
 * Cycle fan to next value
 * Order: Auto -> Low -> Medium -> High -> Auto
 *
 * @param state State to modify
 */
void htw_state_fan_next(HtwState* state);

/**
 * Cycle fan to previous value
 * @param state State to modify
 */
void htw_state_fan_prev(HtwState* state);

/**
 * Increase temperature by 1
 * @param state State to modify
 */
void htw_state_temp_up(HtwState* state);

/**
 * Decrease temperature by 1
 * @param state State to modify
 */
void htw_state_temp_down(HtwState* state);

/**
 * Toggle a boolean state (swing, turbo, fresh, led, clean)
 * @param state State to modify
 * @param toggle Which toggle to flip
 */
void htw_state_toggle(HtwState* state, HtwToggle toggle);

/**
 * Cycle timer ON step
 * @param state State to modify
 * @param forward true for next, false for previous
 */
void htw_state_timer_on_cycle(HtwState* state, bool forward);

/**
 * Cycle timer OFF step
 * @param state State to modify
 * @param forward true for next, false for previous
 */
void htw_state_timer_off_cycle(HtwState* state, bool forward);

/**
 * Check if fan can be changed in current mode
 * (Auto and Dry modes have fixed fan)
 *
 * @param state Current state
 * @return true if fan is adjustable
 */
bool htw_state_can_change_fan(const HtwState* state);

/**
 * Check if temperature can be changed in current mode
 * (Fan-only mode has no temperature)
 *
 * @param state Current state
 * @return true if temperature is adjustable
 */
bool htw_state_can_change_temp(const HtwState* state);

#ifdef __cplusplus
}
#endif
