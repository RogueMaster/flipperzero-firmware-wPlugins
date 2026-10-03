/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* PIV and TWIC signed-object envelopes: CBEFF biometric framing and the
 * CMS profiles for CHUID, biometric and security objects.
 * Standards: SP 800-73-5 Part 1, SP 800-76-2 section 9.3, FIPS 201-3,
 * TWIC Part 2 v5.
 * Configuration: TC_ENABLE_PIV_OBJECTS.
 * Contracts: docs/api.md. Guide: docs/cms.md. */
#ifndef TINY_CRYPTO_PIV_CMS_H_
#define TINY_CRYPTO_PIV_CMS_H_
#include <tiny_crypto/cms.h>
#include <tiny_crypto/piv_oid.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_PIV_CMS_CHUID,
  TC_PIV_CMS_BIOMETRIC,
  /* FIPS 201-1 section 4.4.2: FASC-N required, entryUUID optional. */
  TC_PIV_CMS_BIOMETRIC_LEGACY,
  TC_PIV_CMS_SECURITY
} TC_PIV_CMS_kind;
typedef struct {
  TC_bytes signed_content, record, signature, fascn;
} TC_PIV_CBEFF;

#if TC_ENABLE_PIV_OBJECTS
/* Split the value of a PIV biometric object's BC field using the CBEFF
 * header of SP 800-76-2 section 9.2 and Table 14. The 88-byte header is part
 * of signed_content. Record and CMS signature blocks must be nonempty and
 * consume the complete input. Header version 3 is supported. Biometric
 * format, dates, quality and encryption metadata need
 * TC_PIV_CBEFF_metadata_read and application checks. All spans borrow
 * encoded. encoded and out must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT for NULL out, NULL data with a
 * length, or overlap. UNSUPPORTED for another header version. INVALID for a
 * short header or block lengths that leave the input unfilled. out changes only
 * on OK. */
TC_TLV_result TC_PIV_CBEFF_read(TC_bytes encoded, TC_PIV_CBEFF* out);
#endif

typedef struct {
  TC_X509_time created, valid_from, valid_until;
  TC_bytes creator;
  uint16_t format_owner, format_type;
  uint32_t biometric_type;
  uint8_t data_type;
  int quality, encrypted;
} TC_PIV_CBEFF_metadata;

#if TC_ENABLE_PIV_OBJECTS
/* Read the SP 800-76-2 Table 14 metadata from a complete BC value. Checks the
 * framing of TC_PIV_CBEFF_read, the signed or signed-and-encrypted security
 * options, calendar values and the order creation <= validity start <=
 * validity end, the quality range -2..100, NUL-terminated printable creator
 * text and zero reserved bytes. The creator span borrows encoded. Format
 * identifiers are returned for application interpretation. valid_until is
 * biometric metadata. Credential expiration is an application decision.
 * encoded and out must be disjoint. Charges no work.
 * Returns OK with out written. ARGUMENT and UNSUPPORTED follow
 * TC_PIV_CBEFF_read. INVALID for framing or any metadata check. out changes
 * only on OK. */
TC_TLV_result TC_PIV_CBEFF_metadata_read(TC_bytes encoded, TC_PIV_CBEFF_metadata* out);
#endif

typedef enum {
  TC_PIV_CBEFF_FORMAT_UNKNOWN,
  TC_PIV_CBEFF_FINGERPRINT_IMAGE,
  TC_PIV_CBEFF_FINGERPRINT_TEMPLATE,
  TC_PIV_CBEFF_IRIS_IMAGE,
  TC_PIV_CBEFF_FACE_IMAGE
} TC_PIV_CBEFF_format;

#if TC_ENABLE_PIV_OBJECTS
/* Identify the SP 800-76-2 Table 15 header tuple of format owner, format
 * type, biometric type and processing level. NULL and unrecognized tuples
 * return UNKNOWN. Record contents, encryption and quality need separate
 * checks. Charges no work. */
TC_PIV_CBEFF_format TC_PIV_CBEFF_format_identify(const TC_PIV_CBEFF_metadata* metadata);
#endif

typedef struct {
  TC_CMS_signed_data envelope;
  TC_CMS_signer_info signer;
  TC_CMS_signed_attributes attributes;
  TC_bytes certificate;
} TC_PIV_CMS_object;

#if TC_ENABLE_PIV_OBJECTS
/* Read a single-signer PIV or TWIC CMS profile.
 * - CHUID: SP 800-73-5 Part 1 section 3.1.2.1. Detached content, one
 *   embedded certificate and a version 1 signer.
 * - BIOMETRIC: SP 800-76-2 section 9.3. Detached content, an optional
 *   certificate, and required FASC-N and entryUUID attributes.
 * - BIOMETRIC_LEGACY: FIPS 201-1 section 4.4.2. As BIOMETRIC with an optional
 *   entryUUID.
 * - SECURITY: SP 800-73-5 Part 1 section 3.1.7. Attached LDS content
 *   (1.3.27.1.1.1), no certificate, and an issuer/serial or subject key
 *   identifier signer. Signer name and card identifiers are optional.
 * Every profile needs SignedData version 3, no revocations, matching content
 * type attributes, the signer's digest listed in digestAlgorithms and, except
 * SECURITY, a pivSigner-DN attribute. policy selects the envelope and
 * attribute encodings and the attribute handling. attribute_oids must be
 * TC_CMS_ATTRIBUTE_OIDS_PIV or TC_CMS_ATTRIBUTE_OIDS_PIV_TWIC and also
 * selects the content-type identifiers. PIV_TWIC accepts either identifier of
 * each TWIC Part 2 v5 section 6 pair.
 * - out holds borrowed views, including the optional encoded certificate.
 *   Validate that certificate with TC_X509_read. A biometric signature
 *   without a certificate requires the CHUID signing key.
 * - policy is copied at entry. encoded, limits, frames, work and out must be
 *   disjoint. Requires X.509 and BER support.
 *
 * Work: a 10-unit storage check and the passes of TC_CMS_signed_data_read,
 * the certificate field, TC_CMS_signers_init, TC_CMS_signer_next,
 * TC_CMS_signed_attributes_read and the digest listing.
 * Returns OK with out written. ARGUMENT for an unknown kind, NULL or unknown
 * policy values, another attribute_oids value, NULL storage or overlap, with
 * all state unchanged. LIMIT for exhausted limits, frames or work.
 * UNSUPPORTED for an unknown selected hash or a rejected attribute. INVALID
 * for any profile or schema failure. out changes only on OK. Frames and work
 * are provisional on failure. The application authenticates the content,
 * binds identifiers and validates signer usage, policy and trust, or uses
 * the validators in credential.h. */
TC_TLV_result TC_PIV_CMS_read(TC_bytes encoded, TC_PIV_CMS_kind kind,
                              const TC_CMS_verification_policy* policy, const TC_TLV_limits* limits,
                              TC_TLV_frames frames, size_t* work, TC_PIV_CMS_object* out);

/* Match the signed attributes of an object from TC_PIV_CMS_read against a
 * credential's 25-byte FASC-N and 16-byte CHUID GUID. Select the kind used
 * for reading. BIOMETRIC requires both attributes. BIOMETRIC_LEGACY requires
 * FASC-N and checks entryUUID when present. CHUID and SECURITY check each
 * attribute present and permit either to be absent. Comparisons scan the
 * borrowed OCTET STRING chunks. For CHUID, also bind the identifiers in its
 * signed content to the card authentication certificate.
 * - object and its buffers stay stable. Inputs may overlap each other.
 *   frames, work and matched are disjoint from each other and every input.
 *
 * Work: one unit per storage comparison, then the scanned attribute bytes.
 * Returns OK and writes matched as 1 when every checked attribute matches and
 * 0 otherwise. ARGUMENT for NULL arguments, an unknown kind, identifiers of
 * the wrong length or overlap, with work and matched unchanged. INVALID for a
 * required attribute that is absent, before any work, or for a malformed
 * attribute. LIMIT for exhausted limits, frames or work. Errors leave matched
 * unchanged. Authenticate the object's signature and signer trust before
 * accepting a match. */
TC_TLV_result TC_PIV_CMS_identifiers_match(const TC_PIV_CMS_object* object, TC_PIV_CMS_kind kind,
                                           TC_bytes fascn, TC_bytes uuid,
                                           const TC_TLV_limits* limits, TC_TLV_frames frames,
                                           size_t* work, int* matched);
#endif

#ifdef __cplusplus
}
#endif
#endif
