#pragma once

#include <stdint.h>

uint32_t hf_date_pack(uint16_t year, uint8_t month, uint8_t day);

void hf_date_unpack(uint32_t packed, uint16_t *year, uint8_t *month, uint8_t *day);

int32_t hf_date_diff_days(uint32_t from_packed, uint32_t to_packed);

uint32_t hf_date_add_days(uint32_t packed, int32_t delta);
