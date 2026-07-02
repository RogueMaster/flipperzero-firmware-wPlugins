/**
 * @file PT2257.h
 * @brief PT2257 electronic volume controller (0..79 dB attenuation).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Probe PT2257 on the configured I2C address. */
bool pt2257_is_device_ready(void);

/** Set 8-bit I2C address byte (typical 0x88). */
void pt2257_set_i2c_addr(uint8_t addr);

/** Set attenuation in dB (0 = 0 dB, 79 = −79 dB). */
bool pt2257_set_attenuation_db(uint8_t attenuation_db);

/** Enable or disable mute. */
bool pt2257_mute(bool enable);
