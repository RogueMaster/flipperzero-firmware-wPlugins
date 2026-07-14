#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// IR Protocol timing constants (microseconds)
#define HTW_IR_CARRIER_FREQ 38000
#define HTW_IR_DUTY_CYCLE   0.33f

#define HTW_IR_LEAD_MARK  4400
#define HTW_IR_LEAD_SPACE 4380
#define HTW_IR_BIT_MARK   560
#define HTW_IR_BIT0_SPACE 510
#define HTW_IR_BIT1_SPACE 1590
#define HTW_IR_STOP_MARK  560
#define HTW_IR_GAP        5180

// Frame structure: Leader + 48 bits + Stop + Gap + Leader + 48 bits + Stop
// Each bit = Mark + Space, so 48 bits = 96 timings
// Total per frame: 2 (leader) + 96 (bits) + 1 (stop) + 1 (gap) = 100 timings
// Two frames: 100 + 100 - 1 (no final gap) = 199 timings max
#define HTW_IR_MAX_TIMINGS 200

// Temperature range
#define HTW_TEMP_MIN 17
#define HTW_TEMP_MAX 30

// Timer steps count (34 values from 0.5h to 24h)
#define HTW_TIMER_STEPS_COUNT 34

// Operating modes
typedef enum {
    HtwModeOff = 0,
    HtwModeCool,
    HtwModeAuto,
    HtwModeDry,
    HtwModeHeat,
    HtwModeFan,
    HtwModeCount
} HtwMode;

// Fan speeds
typedef enum {
    HtwFanAuto = 0,
    HtwFanLow,
    HtwFanMedium,
    HtwFanHigh,
    HtwFanCount
} HtwFan;

// Toggle commands
typedef enum {
    HtwTogglePowerOff = 0,
    HtwToggleSwing,
    HtwToggleLed,
    HtwToggleTurbo,
    HtwToggleFresh,
    HtwToggleClean,
    HtwToggleCount
} HtwToggle;

/**
 * Encode a state command (Power ON + set mode/fan/temp)
 * Any state command turns ON the AC and sets parameters
 *
 * @param mode Operating mode (Cool/Heat/Auto/Dry/Fan)
 * @param fan Fan speed
 * @param temp Temperature (17-30)
 * @param timings Output buffer for timings
 * @param timings_count Output: number of timings written
 * @return true on success
 */
bool htw_ir_encode_state(
    HtwMode mode,
    HtwFan fan,
    uint8_t temp,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Encode a toggle command (Power Off, Swing, LED, Turbo, Fresh, Clean)
 * Toggle commands don't require parameters
 *
 * @param toggle Toggle command type
 * @param timings Output buffer for timings
 * @param timings_count Output: number of timings written
 * @return true on success
 */
bool htw_ir_encode_toggle(HtwToggle toggle, uint32_t* timings, size_t* timings_count);

/**
 * Encode Timer ON command
 * Sets a timer to turn ON the AC after specified time with given parameters
 *
 * @param mode Operating mode for when timer fires
 * @param fan Fan speed for when timer fires
 * @param temp Temperature for when timer fires
 * @param timer_step Timer step (1-34, see HTW_TIMER_STEPS for values)
 * @param timings Output buffer for timings
 * @param timings_count Output: number of timings written
 * @return true on success
 */
bool htw_ir_encode_timer_on(
    HtwMode mode,
    HtwFan fan,
    uint8_t temp,
    uint8_t timer_step,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Encode Timer OFF command
 * Sets a timer to turn OFF the AC after specified time
 *
 * @param mode Current operating mode (needed for protocol)
 * @param temp Current temperature (needed for protocol)
 * @param timer_step Timer step (1-34)
 * @param timings Output buffer for timings
 * @param timings_count Output: number of timings written
 * @return true on success
 */
bool htw_ir_encode_timer_off(
    HtwMode mode,
    uint8_t temp,
    uint8_t timer_step,
    uint32_t* timings,
    size_t* timings_count);

/**
 * Get timer value in hours for a given step
 *
 * @param step Timer step (1-34)
 * @return Time in hours (0.5 to 24.0), or 0 if invalid step
 */
float htw_ir_get_timer_hours(uint8_t step);

/**
 * Get mode name string
 */
const char* htw_ir_get_mode_name(HtwMode mode);

/**
 * Get fan speed name string
 */
const char* htw_ir_get_fan_name(HtwFan fan);

#ifdef __cplusplus
}
#endif
