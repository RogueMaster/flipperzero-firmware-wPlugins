#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * PT2257 Electronic Volume Controller driver (Flipper Zero, external I2C).
 *
 * Attenuation is expressed as unsigned integer dB: 0..79 (0 => 0dB, 79 => -79dB).
 * This mirrors the API style in victornpb/evc_pt2257.
 */

bool pt2257_is_device_ready(void);

// NOTE: This driver uses an 8-bit I2C address byte (already left-shifted),
// matching the addressing convention used in Flipper HAL examples for TEA5767.
// Typical PT2257 address byte: 0x88
void pt2257_set_i2c_addr(uint8_t addr);
uint8_t pt2257_get_i2c_addr(void);

bool pt2257_set_attenuation_db(uint8_t attenuation_db);
bool pt2257_set_attenuation_left_db(uint8_t attenuation_db);
bool pt2257_set_attenuation_right_db(uint8_t attenuation_db);

bool pt2257_mute(bool enable);
bool pt2257_off(void);
