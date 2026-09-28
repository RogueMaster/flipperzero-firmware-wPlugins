/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
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

/* Decode the 200-bit FASC-N format in PACS TIG v2.3 sections 6.1-6.3.
 * Checks odd character parity, sentinels, decimal digits and the LRC. Values
 * retain their fixed decimal widths through the field definitions; leading
 * zeros are restored by write. Numeric category values need application policy.
 * Input and out are disjoint. Only OK writes out. */
TC_TLV_result TC_FASCN_read(TC_bytes encoded, TC_FASCN* out);

/* Encode all fields, including parity and LRC. Reject values exceeding their
 * decimal widths. Writes exactly TC_FASCN_BYTES on OK; other results preserve
 * output. value and the entire output range must be disjoint. */
TC_TLV_result TC_FASCN_write(const TC_FASCN* value, uint8_t* out, size_t capacity);

#ifdef __cplusplus
}
#endif
#endif
