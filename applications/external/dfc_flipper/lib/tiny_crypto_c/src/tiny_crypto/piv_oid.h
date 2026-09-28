/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_PIV_OID_H_
#define TINY_CRYPTO_PIV_OID_H_
#include <tiny_crypto/common.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TC_PIV_OIDS_ONLY, TC_PIV_OIDS_TWIC_COMPATIBLE } TC_PIV_oid_profile;
typedef enum {
  TC_PIV_OID_UNKNOWN,
  TC_PIV_OID_POLICY_DIGITAL_SIGNATURE,
  TC_PIV_OID_POLICY_KEY_MANAGEMENT,
  TC_PIV_OID_POLICY_DEVICES,
  TC_PIV_OID_POLICY_AUTHENTICATION,
  TC_PIV_OID_POLICY_CARD_AUTHENTICATION,
  TC_PIV_OID_CHUID_CONTENT,
  TC_PIV_OID_BIOMETRIC_CONTENT,
  TC_PIV_OID_SIGNER_NAME,
  TC_PIV_OID_FASCN,
  TC_PIV_OID_CONTENT_SIGNING,
  TC_PIV_OID_CARD_AUTHENTICATION,
  TC_PIV_OID_BACKGROUND_CHECK,
  TC_PIV_OID_POLICY_CONTENT_SIGNING
} TC_PIV_oid;

/* Classify exact OID contents, excluding the ASN.1 tag and length.
 * TWIC compatibility accepts the pairs in TWIC Part 2 v5, section 6, from
 * either card application. Unlisted, malformed and disabled identifiers return
 * UNKNOWN. Keep original OIDs for signatures and X.509 policy processing.
 * Recognition conveys no trust. Enable TC_ENABLE_PIV_OIDS. */
TC_PIV_oid TC_PIV_oid_identify(TC_bytes oid, TC_PIV_oid_profile profile);

#ifdef __cplusplus
}
#endif
#endif
