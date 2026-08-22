#pragma once

#include "coolix_ir_protocol.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Storage path for state file
#define COOLIX_STATE_FILE_PATH    APP_DATA_PATH("state.txt")
#define COOLIX_STATE_FILE_VERSION 1

// Default values
#define COOLIX_DEFAULT_MODE       CoolixModeCool
#define COOLIX_DEFAULT_FAN        CoolixFanAuto
#define COOLIX_DEFAULT_TEMP       24
#define COOLIX_DEFAULT_SAVE_STATE true

/**
 * Application state structure
 */
typedef struct {
    // Current mode (including Off)
    CoolixMode mode;

    // Last active mode (not Off)
    CoolixMode last_active_mode;

    // Fan speed
    CoolixFan fan;

    // Temperature (17-30)
    uint8_t temp;

    // Toggle states (virtual - the AC sends no feedback)
    bool swing;
    bool turbo;
    bool led;
    bool sleep;

    // Vane position, advanced by the Direct button (display only)
    uint8_t direct_step;

    // Settings
    bool save_state; // Whether to persist state across app restarts
} CoolixState;

/**
 * Allocate and initialize state with defaults
 * @return Allocated state structure
 */
CoolixState* coolix_state_alloc(void);

/**
 * Free state structure
 * @param state State to free
 */
void coolix_state_free(CoolixState* state);

/**
 * Reset state to defaults
 * @param state State to reset
 */
void coolix_state_reset(CoolixState* state);

/**
 * Load state from storage
 * If loading fails or save_state is false, resets to defaults
 *
 * @param state State structure to load into
 * @return true if successfully loaded, false if reset to defaults
 */
bool coolix_state_load(CoolixState* state);

/**
 * Save state to storage
 * If save_state is false, only saves the save_state=false flag
 *
 * @param state State structure to save
 * @return true on success
 */
bool coolix_state_save(CoolixState* state);

/**
 * Set mode directly to a specific value
 * @param state State to modify
 * @param mode Mode to set
 */
void coolix_state_set_mode(CoolixState* state, CoolixMode mode);

/**
 * Set fan directly to a specific value
 * @param state State to modify
 * @param fan Fan speed to set
 */
void coolix_state_set_fan(CoolixState* state, CoolixFan fan);

/**
 * Increase temperature by 1
 * @param state State to modify
 */
void coolix_state_temp_up(CoolixState* state);

/**
 * Decrease temperature by 1
 * @param state State to modify
 */
void coolix_state_temp_down(CoolixState* state);

/**
 * Toggle a boolean state (swing, turbo, led, sleep) or advance the vane step
 * @param state State to modify
 * @param toggle Which button was pressed
 */
void coolix_state_toggle(CoolixState* state, CoolixToggle toggle);

/**
 * Check if fan can be changed in current mode
 * (Auto and Dry modes have fixed fan)
 *
 * @param state Current state
 * @return true if fan is adjustable
 */
bool coolix_state_can_change_fan(const CoolixState* state);

/**
 * Check if temperature can be changed in current mode
 * (Fan-only mode has no temperature)
 *
 * @param state Current state
 * @return true if temperature is adjustable
 */
bool coolix_state_can_change_temp(const CoolixState* state);

/**
 * Build the 24-bit word the current state would transmit.
 * Used by the Extra screen to show what is going out.
 *
 * @param state Current state
 * @return 24-bit Coolix command word
 */
uint32_t coolix_state_current_code(const CoolixState* state);

#ifdef __cplusplus
}
#endif
