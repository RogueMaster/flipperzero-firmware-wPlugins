#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

uint8_t seader_calc_lrc(uint8_t* data, size_t len);
bool seader_validate_lrc(uint8_t* data, size_t len);
size_t seader_add_lrc(uint8_t* data, size_t len);
