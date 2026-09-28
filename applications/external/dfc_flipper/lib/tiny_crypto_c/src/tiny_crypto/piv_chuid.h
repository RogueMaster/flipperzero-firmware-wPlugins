/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_CHUID_H_
#define TINY_CRYPTO_PIV_CHUID_H_
#include <tiny_crypto/tlv.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_PIV_CHUID_CONTENTS, TC_PIV_CHUID_CONTAINER } TC_PIV_CHUID_encoding;
typedef enum {
  TC_CHUID_PROFILE_PIV, TC_CHUID_PROFILE_TWIC_SIGNED, TC_CHUID_PROFILE_TWIC_UNSIGNED,
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

/* CONTAINER includes the outer 53 object; CONTENTS starts with its first field.
 * Spans borrow the input; input and out must be disjoint. out is unchanged on
 * failure. An absent optional cardholder UUID has a NULL pointer.
 * The CMS signature is returned unverified. */
TC_TLV_result TC_PIV_CHUID_read(const uint8_t* data, size_t length,
                               TC_PIV_CHUID_encoding encoding, TC_PIV_CHUID* out);
/* Select the profile according to the application's card-object policy.
 * Unsigned TWIC has neither a signature nor a cardholder UUID.
 * LEGACY_KEY_MAP permits an optional 3D field (0..512 bytes) immediately before
 * the signature in a PIV-shaped CHUID. signed_content includes its exact
 * encoded TLV. Authenticate the signature before using the map.
 * The field definition follows SP 800-73-2 Part 1 Table 8. */
TC_TLV_result TC_PIV_CHUID_read_profile(const uint8_t* data, size_t length,
    TC_PIV_CHUID_encoding encoding, TC_PIV_CHUID_profile profile, TC_PIV_CHUID* out);

#ifdef __cplusplus
}
#endif
#endif
