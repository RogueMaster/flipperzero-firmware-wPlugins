/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* ANSI AAMVA barcode payloads: subfile directory and text field lookup for
 * the DL/ID data carried in PDF417 barcodes, including TWIC barcodes.
 * Standards: AAMVA DL/ID Card Design Standard 2025 Annex D.
 * Configuration: TC_ENABLE_AAMVA. Limitations: image decoding, PDF417
 * recognition and field interpretation belong to the application.
 * Contracts: docs/api.md. Guide: docs/twic-barcode.md. */
#ifndef TINY_CRYPTO_AAMVA_H_
#define TINY_CRYPTO_AAMVA_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

#if TC_ENABLE_AAMVA
/* Find a two-letter subfile designator in an ANSI AAMVA payload of version 01
 * or later (AAMVA DL/ID Card Design Standard 2025 sections D.12.3 and
 * D.12.4). Checks the header, the 1..99 directory entries, their uppercase
 * types, offsets and lengths, duplicate types, overlapping ranges and each
 * subfile's leading type and final CR. out borrows the full subfile,
 * including its designator and CR. encoded, designator and out must be
 * disjoint. The directory check compares each entry with the earlier ones,
 * so its cost grows with the square of the entry count. Charges no work.
 * Returns OK with out written. END when the designator is absent. ARGUMENT
 * for NULL arguments, NULL data with a length, a designator outside A..Z or
 * overlap. UNSUPPORTED for AAMVA version 00. INVALID for any header or
 * directory failure. out changes only on OK. */
TC_TLV_result TC_AAMVA_subfile_find(TC_bytes encoded, const char designator[2], TC_bytes* out);

/* Find a three-letter data element in a complete text subfile from
 * TC_AAMVA_subfile_find (AAMVA DL/ID Card Design Standard 2025 section
 * D.12.5). Accepts LF-separated printable ASCII values, an optional LF after
 * the subfile type and a final CR. Empty values are valid. out borrows the
 * value without separators. subfile, identifier and out must be disjoint.
 * Charges no work.
 * Returns OK with out written. END when the element is absent. ARGUMENT for
 * NULL arguments, NULL data with a length, an identifier outside A..Z or
 * overlap. INVALID for a malformed subfile or element, or a repeated
 * requested element. out changes only on OK. Field interpretation belongs to
 * the application. */
TC_TLV_result TC_AAMVA_field_find(TC_bytes subfile, const char identifier[3], TC_bytes* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
