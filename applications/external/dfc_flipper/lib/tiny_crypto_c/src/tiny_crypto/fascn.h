/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* FASC-N readers and writers for the 200-bit PACS encoding.
 * Standards: PACS TIG v2.3 sections 6.1-6.3.
 * Configuration: TC_ENABLE_FASCN.
 * Limitations: category and other numeric values need application policy.
 * Contracts: docs/api.md. Guide: docs/fascn.md. */
#ifndef TINY_CRYPTO_FASCN_H_
#define TINY_CRYPTO_FASCN_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { TC_FASCN_BYTES = 25 };
typedef struct {
  uint64_t person;
  uint32_t credential;
  uint16_t agency, system, organization;
  uint8_t series, issue, category, association;
} TC_FASCN;

#if TC_ENABLE_FASCN
/* Decode the 200-bit FASC-N in PACS TIG v2.3 sections 6.1-6.3. Checks odd
 * character parity, the start, separator and end sentinels, decimal digits
 * and the LRC. Values keep their fixed decimal widths through the field
 * definitions, and write restores leading zeros. Numeric category values need
 * application policy. encoded and out must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out, NULL data with a
 * length, or overlap. INVALID for a length other than TC_FASCN_BYTES or any
 * failed check. out changes only on OK. */
TC_TLV_result TC_FASCN_read(TC_bytes encoded, TC_FASCN* out);

/* Encode all fields, including parity and LRC, as exactly TC_FASCN_BYTES
 * bytes. value and the entire output range must be disjoint. Charges no work.
 * Returns OK with 25 bytes written. ARGUMENT for NULL arguments, overlap or
 * a field above its decimal width, checked before the capacity. LIMIT for a
 * capacity below TC_FASCN_BYTES. out changes only on OK. */
TC_TLV_result TC_FASCN_write(const TC_FASCN* value, TC_buffer out);
#endif

#ifdef __cplusplus
}
#endif
#endif
