/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* TWIC NEXGEN card UUID readers, writers and FASC-N comparison.
 * Standards: TWIC Part 2 v5 appendix D.
 * Configuration: TC_ENABLE_TWIC_UUID, which requires TC_ENABLE_FASCN.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_TWIC_UUID_H_
#define TINY_CRYPTO_TWIC_UUID_H_
#include <tiny_crypto/fascn.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { TC_TWIC_UUID_BYTES = 16 };
#if TC_ENABLE_TWIC_UUID
/* Read a NEXGEN card UUID (TWIC Part 2 v5 Appendix D). number is the 14-digit
 * decimal concatenation of agency, system and credential, stored in the
 * final 48 bits. The first 10 bytes hold the fixed namespace, version and
 * variant. encoded and number must be disjoint. Charges no work.
 * Returns OK with number written. ARGUMENT for NULL number, NULL data with a
 * length, or overlap. INVALID for a length other than 16, another prefix or
 * a number above 99999999999999. number changes only on OK. */
TC_TLV_result TC_TWIC_uuid_read(TC_bytes encoded, uint64_t* number);

/* Write the NEXGEN UUID of a number in 0..99999999999999 as exactly
 * TC_TWIC_UUID_BYTES bytes. Charges no work.
 * Returns OK with 16 bytes written. ARGUMENT for NULL out, a capacity that
 * wraps the address space or a number above the range, checked before the
 * capacity. LIMIT for a capacity below 16. out changes only on OK. */
TC_TLV_result TC_TWIC_uuid_write(uint64_t number, TC_buffer out);

/* Compare a NEXGEN UUID with the agency, system and credential fields of a
 * decoded FASC-N. The UUID holds only those three fields. Select
 * this policy for TWIC application identifiers. PIV-I placeholder FASC-Ns
 * need separate application handling. encoded and fascn must be disjoint from
 * matched. Charges no work.
 * Returns OK and writes matched as 1 for equal numbers and 0 otherwise.
 * ARGUMENT for NULL arguments or overlap. INVALID for FASC-N fields above
 * their decimal widths or a UUID that TC_TWIC_uuid_read rejects. matched
 * changes only on OK. */
TC_TLV_result TC_TWIC_uuid_match(TC_bytes encoded, const TC_FASCN* fascn, int* matched);
#endif

#ifdef __cplusplus
}
#endif
#endif
