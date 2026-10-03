/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Registered PIV and TWIC object identifiers and their paired classification.
 * Standards: FIPS 201-3, TWIC Part 2 v5 section 6.
 * Configuration: TC_ENABLE_PIV_OIDS.
 * Limitations: unlisted aliases classify as unknown.
 * Contracts: docs/api.md. */
#ifndef TINY_CRYPTO_PIV_OID_H_
#define TINY_CRYPTO_PIV_OID_H_
#include <tiny_crypto/common.h>
#ifdef __cplusplus
extern "C" {
#endif

/* TC_PIV_OIDS_ONLY accepts identifiers under the PIV root 2.16.840.1.101.3.
 * TC_PIV_OIDS_TWIC_COMPATIBLE also accepts the TWIC root 1.3.6.1.4.1.29138. */
typedef enum { TC_PIV_OIDS_ONLY, TC_PIV_OIDS_TWIC_COMPATIBLE } TC_PIV_oid_profile;

/* Identifiers from TWIC Part 2 v5 section 6. Each value names one PIV/TWIC
 * pair. The comments give the PIV identifier, then its TWIC pair. A value with
 * one identifier has no pair in section 6. */
typedef enum {
  TC_PIV_OID_UNKNOWN,
  /* Certificate policies. */
  TC_PIV_OID_POLICY_DIGITAL_SIGNATURE,   /* id-TWIC-digital-signature (TWIC only) */
  TC_PIV_OID_POLICY_COMMON,              /* id-fpki-common-policy, id-TWIC-key-management */
  TC_PIV_OID_POLICY_DEVICES,             /* id-fpki-common-devices, id-TWIC-devices */
  TC_PIV_OID_POLICY_AUTHENTICATION,      /* id-fpki-common-authentication, id-TWIC-authentication */
  TC_PIV_OID_POLICY_CARD_AUTHENTICATION, /* id-fpki-common-cardAuth, id-TWIC-cardAuth */
  /* CMS content types and signed attributes (FIPS 201-3 Tables B-1, B-2). */
  TC_PIV_OID_CHUID_CONTENT,     /* id-PIV-CHUIDSecurityObject (PIV only) */
  TC_PIV_OID_BIOMETRIC_CONTENT, /* id-PIV-biometricObject (PIV only) */
  TC_PIV_OID_SIGNER_NAME,       /* pivSigner-DN (PIV only) */
  TC_PIV_OID_FASCN,             /* pivFASC-N, twicFASC-N */
  /* Extended key usages. */
  TC_PIV_OID_CONTENT_SIGNING,     /* id-PIV-content-signing, id-TWIC-content-signing */
  TC_PIV_OID_CARD_AUTHENTICATION, /* id-PIV-cardAuth, id-TWIC-cardAuth */
  /* Certificate extension. */
  TC_PIV_OID_BACKGROUND_CHECK, /* id-PIV-NACI, id-TWIC-interim */
  /* id-fpki-common-piv-contentSigning, Common Policy section 1.2 (PIV only). */
  TC_PIV_OID_POLICY_CONTENT_SIGNING
} TC_PIV_oid;

#if TC_ENABLE_PIV_OIDS
/* Classify exact OID contents, excluding the ASN.1 tag and length.
 * TWIC compatibility accepts the pairs in TWIC Part 2 v5 section 6 from
 * either card application. TC_PIV_OIDS_ONLY accepts the PIV identifier of
 * each pair and the PIV-only entries. oid is borrowed for the call. Charges
 * no work. Returns the matching value, or UNKNOWN for unlisted or malformed
 * contents, NULL data, a TWIC identifier under TC_PIV_OIDS_ONLY, or an
 * unknown profile. Keep original OIDs for signatures and X.509 policy
 * processing. Recognition conveys no trust. */
TC_PIV_oid TC_PIV_oid_identify(TC_bytes oid, TC_PIV_oid_profile profile);
#endif

#ifdef __cplusplus
}
#endif
#endif
