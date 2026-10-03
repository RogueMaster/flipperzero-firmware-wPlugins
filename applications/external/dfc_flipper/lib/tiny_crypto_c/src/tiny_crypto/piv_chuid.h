/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Card Holder Unique Identifier reader for the PIV, signed and unsigned TWIC,
 * and legacy key-map profiles.
 * Standards: SP 800-73-4 Part 1 Table 9, SP 800-73-2 Part 1 Table 8,
 * TWIC Part 2 v5.
 * Configuration: TC_ENABLE_PIV_CHUID.
 * Limitations: parsing only. credential.h authenticates the signature.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_CHUID_H_
#define TINY_CRYPTO_PIV_CHUID_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_PIV_CHUID_CONTENTS, TC_PIV_CHUID_CONTAINER } TC_PIV_CHUID_encoding;
typedef enum {
  TC_CHUID_PROFILE_PIV,
  TC_CHUID_PROFILE_TWIC_SIGNED,
  TC_CHUID_PROFILE_TWIC_UNSIGNED,
  TC_CHUID_PROFILE_LEGACY_KEY_MAP
} TC_PIV_CHUID_profile;
typedef struct {
  TC_bytes fascn, card_uuid, cardholder_uuid, expiration, signature;
  /* Optional 3D value, covered by signed_content. NULL means absent. */
  TC_bytes authentication_key_map;
  /* Hash these spans in order for CMS detached content. Both are empty for
   * unsigned TWIC. Encodings include the trailing FE field. */
  TC_bytes signed_content[2];
} TC_PIV_CHUID;

#if TC_ENABLE_PIV_CHUID
/* Read a CHUID. CONTAINER includes the outer 53 object. CONTENTS starts with
 * its first field. Select the profile from the application's card-object
 * policy. Field order and sizes follow the profile:
 * - PIV: SP 800-73-4 Part 1 Table 9. The deprecated Buffer Length (EE, 2
 *   bytes), Organizational Identifier (32, 4 bytes) and DUNS (33, 9 bytes) are
 *   optional. Their values are checked for length and otherwise ignored.
 * - TWIC_SIGNED and TWIC_UNSIGNED: TWIC Part 2 sections 4.6.3 and 4.6.1.
 *   Unsigned TWIC has neither a signature nor a cardholder UUID. EE, 32 and 33
 *   are rejected.
 * - LEGACY_KEY_MAP: the PIV fields plus an optional Authentication Key Map
 *   (3D, 0..512 bytes) immediately before the signature, following
 *   SP 800-73-2 Part 1 Table 8. signed_content includes its exact encoded
 *   TLV. Authenticate the signature before using the map.
 * signed_content excludes a Buffer Length element (SP 800-73-4 Part 1
 * section 3.1.2). Spans borrow encoded, which must stay unchanged while they
 * are used. An absent optional field has a NULL pointer. The caller verifies
 * the CMS signature, or uses TC_PIV_CHUID_validate. Charges no work.
 * Returns OK, MORE when input ends inside the outer 53 object or a CONTENTS
 * field, INVALID for malformed or out-of-profile fields, and ARGUMENT for a
 * NULL out, NULL data with a length, an unknown encoding or profile, or out
 * overlapping encoded.
 * out changes only on OK. */
TC_TLV_result TC_PIV_CHUID_read(TC_bytes encoded, TC_PIV_CHUID_encoding encoding,
                                TC_PIV_CHUID_profile profile, TC_PIV_CHUID* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
