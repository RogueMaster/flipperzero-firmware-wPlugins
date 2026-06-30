#pragma once

#include <stdint.h>

uint32_t mp_flipper_get_timestamp();

uint32_t mp_flipper_get_tick_frequency();

uint32_t mp_flipper_get_tick();

void mp_flipper_delay_ms(uint32_t ms);

void mp_flipper_delay_us(uint32_t us);
