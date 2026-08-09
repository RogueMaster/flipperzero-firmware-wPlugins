/**
 * RollCall - pure text parsing over decoder output.
 *
 * Deliberately free of furi and of the Sub-GHz stack: the decoders hand us a
 * human-readable dump, and pulling numbers back out of it is string work that
 * deserves to be testable on a laptop rather than only on the hardware.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lift the bit length out of a decoder dump.
 *
 * Every Sub-GHz decoder opens its string with "<Protocol> <N>bit", so the
 * number in front of the first "bit" token is the frame length. Reading it
 * back out of the text beats reaching into the decoder struct, whose layout is
 * private and differs per protocol.
 *
 * @return the bit length, or 0 if the dump has none we can trust.
 */
uint16_t rc_bits_from_dump(const char* dump);

#ifdef __cplusplus
}
#endif
