#pragma once

#include <stdbool.h>
#include <stdint.h>

void pt2259_set_i2c_addr(uint8_t addr);
uint8_t pt2259_get_i2c_addr(void);

bool pt2259_is_device_ready(void);
bool pt2259_init(void);
bool pt2259_set_attenuation_db(uint8_t attenuation_db);
bool pt2259_set_mute(bool enable);
