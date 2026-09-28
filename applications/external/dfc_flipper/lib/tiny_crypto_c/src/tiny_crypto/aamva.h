/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_AAMVA_H_
#define TINY_CRYPTO_AAMVA_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Find a two-byte subfile designator in an ANSI AAMVA payload (version 01+).
 * Checks the complete directory, including duplicate types and overlapping
 * ranges. On OK, out borrows the full subfile, including its designator and CR.
 * END means absent. All other results preserve out. Input and out are disjoint.
 * Image decoding and field interpretation are supplied by the application. */
TC_TLV_result TC_AAMVA_subfile_find(TC_bytes encoded, const char designator[2], TC_bytes* out);

/* Find a three-letter field in a complete text subfile returned above.
 * Accepts LF-separated printable ASCII values, an optional LF after the subfile
 * type, and a final CR. Empty values are valid. Duplicate requested fields and
 * malformed fields return INVALID. OK borrows the value without separators;
 * END means absent. out must be separate from subfile and identifier; it changes
 * only on OK. */
TC_TLV_result TC_AAMVA_field_find(TC_bytes subfile, const char identifier[3], TC_bytes* out);

#ifdef __cplusplus
}
#endif
#endif
