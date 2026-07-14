/**
 * @file PT2259.h
 * @brief PT2259 electronic volume controller.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Set 8-bit I2C address byte. */
void pt2259_set_i2c_addr(uint8_t addr);

/** Probe PT2259 on the configured I2C address. */
bool pt2259_is_device_ready(void);

/** Initialize PT2259 after address is set. */
bool pt2259_init(void);

/** Set attenuation in dB. */
bool pt2259_set_attenuation_db(uint8_t attenuation_db);

/** Enable or disable mute. */
bool pt2259_set_mute(bool enable);

/** Apply attenuation and mute in one transaction. */
bool pt2259_apply_state(uint8_t attenuation_db, bool muted);
