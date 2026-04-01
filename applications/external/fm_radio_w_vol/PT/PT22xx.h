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

void pt22xx_set_chip(PT22xxChip chip);
PT22xxChip pt22xx_get_chip(void);
const char* pt22xx_get_chip_name(void);

void pt22xx_set_i2c_addr(uint8_t addr);
uint8_t pt22xx_get_i2c_addr(void);

bool pt22xx_is_device_ready(void);
bool pt22xx_init(void);
bool pt22xx_apply_state(const PT22xxState* state);
