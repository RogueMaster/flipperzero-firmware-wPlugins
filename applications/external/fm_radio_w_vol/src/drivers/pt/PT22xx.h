/**
 * @file PT22xx.h
 * @brief Unified volume IC facade (PT2257 or PT2259 over external I2C).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PT22xxChipPT2257 = 0,
    PT22xxChipPT2259 = 1,
} PT22xxChip;

typedef struct {
    uint8_t attenuation_db;
    bool muted;
} PT22xxState;

/** Select active chip implementation for subsequent calls. */
void pt22xx_set_chip(PT22xxChip chip);

/** Set 8-bit I2C address byte (left-shifted, Flipper HAL style). */
void pt22xx_set_i2c_addr(uint8_t addr);

/** Probe whether the selected chip responds on I2C. */
bool pt22xx_is_device_ready(void);

/** Initialize the selected chip. */
bool pt22xx_init(void);

/** Apply attenuation and mute from @p state. */
bool pt22xx_apply_state(const PT22xxState* state);
