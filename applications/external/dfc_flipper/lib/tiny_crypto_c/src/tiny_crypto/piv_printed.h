/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV printed information and decrypted TWIC DFC109 reader.
 * Standards: SP 800-73-5 Part 1, TWIC Part 2 v5.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Limitations: parsing only. Expiration checks follow CHUID authentication.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_PRINTED_H_
#define TINY_CRYPTO_PIV_PRINTED_H_
#include <tiny_crypto/tlv.h>
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_PIV_PRINTED_CONTENTS, TC_PIV_PRINTED_CONTAINER } TC_PIV_printed_encoding;

typedef enum { TC_PIV_PRINTED_PROFILE_PIV, TC_PIV_PRINTED_PROFILE_TWIC } TC_PIV_printed_profile;

typedef struct {
  TC_bytes name;
  TC_bytes employee_affiliation;
  TC_bytes expiration_text;
  TC_bytes card_serial_number;
  TC_bytes issuer_identification;
  TC_bytes organization_1;
  TC_bytes organization_2;
  TC_X509_time expiration;
} TC_PIV_printed;

#if TC_ENABLE_PIV_OBJECTS
/* Parse a PIV Printed Information object (SP 800-73-5 Part 1 section 3.3.1
 * and Table 15) or decrypted TWIC DFC109 contents (TWIC Part 2 v5 section
 * 4.7.2). CONTAINER includes the outer 53 object and CONTENTS starts with the
 * first field.
 * - PIV: name (1..125 printable bytes), employee affiliation (0..20),
 *   expiration YYYYMMMDD, card serial (1..20), 15-byte issuer, optional
 *   organization lines (0..20 each) and an empty FE.
 * - TWIC: contents of at most 200 bytes with the same text fields, an
 *   expiration DDMMMYYYY, an 8-digit serial, an 8-digit issuer starting
 *   7099, both organization lines and no FE.
 * Month names are upper case. expiration holds the date at 00:00:00 UTC.
 * Spans borrow input. input and out must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out, NULL data with a
 * length, an unknown encoding or profile, or overlap. LIMIT for TWIC
 * contents above 200 bytes. MORE when input ends inside the outer 53 object.
 * INVALID for a missing, reordered or out-of-profile field, bad text or date,
 * or trailing bytes. out changes only on OK. Parsing supplies structure.
 * Authentication comes from the Security Object. */
TC_TLV_result TC_PIV_printed_read(TC_bytes input, TC_PIV_printed_encoding encoding,
                                  TC_PIV_printed_profile profile, TC_PIV_printed* out);

/* Require the printed expiration date to equal the authenticated CHUID
 * expiration (8 bytes, YYYYMMDD) and at to fall on or before the last second
 * of that day in UTC. printed, chuid_expiration and at are borrowed for the
 * call. valid must be disjoint from them. Charges no work.
 * Returns OK and writes valid as 1 when both checks pass and 0 otherwise.
 * ARGUMENT for NULL arguments or a CHUID expiration without 8 bytes.
 * INVALID for a malformed CHUID date or an invalid time. valid changes only
 * on OK. */
TC_TLV_result TC_PIV_printed_expiration_check(const TC_PIV_printed* printed,
                                              TC_bytes chuid_expiration, const TC_X509_time* at,
                                              int* valid);
#endif

#ifdef __cplusplus
}
#endif
#endif
