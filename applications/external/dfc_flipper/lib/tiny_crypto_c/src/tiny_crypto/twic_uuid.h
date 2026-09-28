/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_TWIC_UUID_H_
#define TINY_CRYPTO_TWIC_UUID_H_
#include <tiny_crypto/fascn.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { TC_TWIC_UUID_BYTES = 16 };
/* NEXGEN UUID, TWIC Part 2 v5 Appendix D. number is the 14-digit decimal
 * concatenation of agency, system and credential, stored as a 48-bit integer.
 * Read checks the fixed namespace/version/variant and numeric range. Input and
 * number must be disjoint. Only OK changes output. */
TC_TLV_result TC_TWIC_uuid_read(TC_bytes encoded, uint64_t* number);

/* Write a NEXGEN UUID for a number in 0..99999999999999. Writes exactly 16
 * bytes on OK, preserving output otherwise. A short buffer returns LIMIT. */
TC_TLV_result TC_TWIC_uuid_write(uint64_t number, uint8_t* out, size_t capacity);

/* Compare a NEXGEN UUID with a decoded FASC-N's first three fields. The other
 * fields do not occur in the UUID. Select this policy for TWIC application
 * identifiers; PIV-I placeholder FASC-Ns require separate application handling.
 * All inputs must be disjoint from matched. OK writes 0 or 1; errors preserve it. */
TC_TLV_result TC_TWIC_uuid_match(TC_bytes encoded, const TC_FASCN* fascn, int* matched);

#ifdef __cplusplus
}
#endif
#endif
