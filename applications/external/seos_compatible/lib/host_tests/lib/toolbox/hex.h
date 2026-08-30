/* Host stand-in for the toolbox hex helper. */
#pragma once

#include <stdint.h>

/* `length` is the number of output characters, not input bytes, and the
 * result is not terminated. Same contract as the firmware's. */
void uint8_to_hex_chars(const uint8_t* src, uint8_t* target, int length);
