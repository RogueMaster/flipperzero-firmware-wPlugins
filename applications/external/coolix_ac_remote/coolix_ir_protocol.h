#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --------------------------------------------------------------------------
// Coolix IR protocol (24-bit)
//
// Port of ESPHome's coolix component:
//   esphome/components/coolix/coolix.cpp          (state word composition)
//   esphome/components/remote_base/coolix_protocol.cpp  (line coding)
// Extra one-shot codes come from IRremoteESP8266's ir_Coolix.h.
//
// Wire format, all times are multiples of a 560 us tick:
//   [8T mark][8T space] then 3 payload bytes, MSB byte first. Every byte is
//   sent normally and then again inverted, MSB bit first. [1T mark] closes the
//   frame. The whole frame is sent twice, separated by a 10T space.
// --------------------------------------------------------------------------

// IR carrier
#define COOLIX_IR_CARRIER_FREQ 38000
#define COOLIX_IR_DUTY_CYCLE   0.33f

// Timings (microseconds)
#define COOLIX_IR_TICK       560
#define COOLIX_IR_LEAD_MARK  (8 * COOLIX_IR_TICK)
#define COOLIX_IR_LEAD_SPACE (8 * COOLIX_IR_TICK)
#define COOLIX_IR_BIT_MARK   (1 * COOLIX_IR_TICK)
#define COOLIX_IR_BIT1_SPACE (3 * COOLIX_IR_TICK)
#define COOLIX_IR_BIT0_SPACE (1 * COOLIX_IR_TICK)
#define COOLIX_IR_STOP_MARK  (1 * COOLIX_IR_TICK)
#define COOLIX_IR_GAP        (10 * COOLIX_IR_TICK)

// Frame: 2 (leader) + 3 bytes * 16 bits * 2 timings (96) + 1 (stop) = 99
// Signal: frame + gap + frame = 99 + 1 + 99 = 199
#define COOLIX_IR_MAX_TIMINGS 200

// Temperature range (Celsius)
#define COOLIX_TEMP_MIN 17
#define COOLIX_TEMP_MAX 30

// Operating modes
typedef enum {
    CoolixModeOff = 0,
    CoolixModeCool,
    CoolixModeAuto,
    CoolixModeDry,
    CoolixModeHeat,
    CoolixModeFan,
    CoolixModeCount
} CoolixMode;

// Fan speeds
typedef enum {
    CoolixFanAuto = 0,
    CoolixFanLow,
    CoolixFanMedium,
    CoolixFanHigh,
    CoolixFanCount
} CoolixFan;

// One-shot buttons shown on the main screen
typedef enum {
    CoolixTogglePowerOff = 0,
    CoolixToggleSwing, // vane oscillation on/off
    CoolixToggleDirect, // step the vertical vane to the next position
    CoolixToggleTurbo,
    CoolixToggleLed,
    CoolixToggleSleep,
    CoolixToggleCount
} CoolixToggle;

// Less common codes, kept on the Extra screen
typedef enum {
    CoolixExtraSilence = 0, // "Silence" / Feeling-Point on some units
    CoolixExtraSwingH, // horizontal vane
    CoolixExtraFanCmd, // fan-speed-only command
    CoolixExtraClean,
    CoolixExtraCount
} CoolixExtra;

/**
 * Build the 24-bit state word for a mode/fan/temperature combination.
 * Returns the power-off word when mode is CoolixModeOff.
 */
uint32_t coolix_ir_build_state(CoolixMode mode, CoolixFan fan, uint8_t temp);

/** 24-bit word behind a main-screen button. */
uint32_t coolix_ir_get_toggle_code(CoolixToggle toggle);

/** 24-bit word behind an Extra-screen entry. */
uint32_t coolix_ir_get_extra_code(CoolixExtra extra);

/**
 * Encode an arbitrary 24-bit Coolix word into Flipper raw IR timings.
 *
 * @param code 24-bit command word
 * @param timings Output buffer, at least COOLIX_IR_MAX_TIMINGS entries
 * @param timings_count Output: number of timings written
 * @return true on success
 */
bool coolix_ir_encode_code(uint32_t code, uint32_t* timings, size_t* timings_count);

/**
 * Encode a state command (power on + mode/fan/temperature).
 * @return true on success, false for CoolixModeOff (use the toggle instead)
 */
bool coolix_ir_encode_state(
    CoolixMode mode,
    CoolixFan fan,
    uint8_t temp,
    uint32_t* timings,
    size_t* timings_count);

/** Encode a main-screen one-shot button. */
bool coolix_ir_encode_toggle(CoolixToggle toggle, uint32_t* timings, size_t* timings_count);

/** Encode an Extra-screen one-shot command. */
bool coolix_ir_encode_extra(CoolixExtra extra, uint32_t* timings, size_t* timings_count);

/** Display name for a mode. */
const char* coolix_ir_get_mode_name(CoolixMode mode);

/** Display name for a fan speed. */
const char* coolix_ir_get_fan_name(CoolixFan fan);

/** Short payload string for a main-screen button, for the Extra screen. */
void coolix_ir_format_toggle(CoolixToggle toggle, char* out, size_t len);

/** Display name for an Extra-screen entry. */
const char* coolix_ir_get_extra_name(CoolixExtra extra);

#ifdef __cplusplus
}
#endif
