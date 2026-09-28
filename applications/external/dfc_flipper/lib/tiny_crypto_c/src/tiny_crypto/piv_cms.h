/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
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

/* Split the value of a PIV biometric object's BC field using SP 800-76-2
 * Table 14. The 88-byte header is included in signed_content. Record and CMS
 * signature blocks must be nonempty and consume the complete input.
 * Header version 3 is supported. Biometric format, dates, quality and encryption
 * metadata require application checks. All returned spans borrow input.
 * Input and out must be disjoint; out changes only on OK. */
TC_TLV_result TC_PIV_CBEFF_read(TC_bytes encoded, TC_PIV_CBEFF* out);

typedef struct {
  TC_X509_time created, valid_from, valid_until;
  TC_bytes creator;
  uint16_t format_owner, format_type;
  uint32_t biometric_type;
  uint8_t data_type;
  int quality, encrypted;
} TC_PIV_CBEFF_metadata;

/* Read Table 14 metadata from a complete BC value. Checks calendar values and
 * their order, security options, quality range, creator text and reserved bytes.
 * The creator span borrows input. Format identifiers are returned for application
 * interpretation. valid_until is biometric metadata; this API makes no decision
 * about credential expiration. Input and out are disjoint; errors preserve out. */
TC_TLV_result TC_PIV_CBEFF_metadata_read(TC_bytes encoded, TC_PIV_CBEFF_metadata* out);

typedef enum {
  TC_PIV_CBEFF_FORMAT_UNKNOWN,
  TC_PIV_CBEFF_FINGERPRINT_IMAGE,
  TC_PIV_CBEFF_FINGERPRINT_TEMPLATE,
  TC_PIV_CBEFF_IRIS_IMAGE,
  TC_PIV_CBEFF_FACE_IMAGE
} TC_PIV_CBEFF_format;

/* Identify SP 800-76-2 Table 15 header tuples. NULL and unrecognized tuples
 * return UNKNOWN. Record contents, encryption and quality need separate checks. */
TC_PIV_CBEFF_format TC_PIV_CBEFF_format_identify(const TC_PIV_CBEFF_metadata* metadata);

typedef struct {
  TC_CMS_signed_data envelope;
  TC_CMS_signer_info signer;
  TC_CMS_signed_attributes attributes;
  TC_bytes certificate;
} TC_PIV_CMS_object;

/* Read a single-signer PIV/TWIC CMS profile. External signatures follow
 * SP 800-73-5 Part 1 section 3.1.2.1
 * or SP 800-76-2 section 9.3. BIOMETRIC requires FASC-N and entryUUID;
 * BIOMETRIC_LEGACY follows FIPS 201-1 section 4.4.2 and requires FASC-N.
 * SECURITY follows section 3.1.7: attached LDS content (1.3.27.1.1.1) and an omitted signing
 * certificate. Both issuer/serial and subject-key-ID signers are supported.
 * Its signer name and card identifiers are optional signed attributes.
 * Select accepted OID namespaces and attribute encoding independently.
 * The signer's digest must be recognized and listed in digestAlgorithms with
 * valid parameters. Unknown selected hashes return UNSUPPORTED.
 *
 * Returns borrowed views, including the optional encoded certificate. Validate
 * that certificate with TC_X509_read. The application must authenticate the
 * content, bind identifiers, and validate signer usage, policy and trust.
 * A biometric signature without a certificate requires the CHUID signing key.
 *
 * Input, limits, frames, work and out must be disjoint. Bad storage preserves
 * caller state. Other errors may consume work/scratch; out changes only on OK.
 * Requires X509 and BER support. */
TC_TLV_result TC_PIV_CMS_read(TC_bytes encoded, TC_PIV_CMS_kind kind,
    TC_PIV_oid_profile oids, TC_CMS_attribute_encoding attributes,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t frame_capacity,
    size_t* work, TC_PIV_CMS_object* out);

/* Match signed attributes against a credential's 25-byte FASC-N and
 * 16-byte CHUID GUID. object must come from TC_PIV_CMS_read with stable buffers.
 * Select the same kind used for reading. BIOMETRIC requires both attributes;
 * BIOMETRIC_LEGACY requires FASC-N and checks entryUUID when present.
 * CHUID and SECURITY check each attribute present and permit either to be absent.
 * Comparisons scan borrowed OCTET STRING chunks. For CHUID, also bind the
 * identifiers in its signed content to the card-authentication certificate.
 * OK writes matched as 0 or 1; all errors preserve it. Authenticate the object's
 * signature and signer trust before accepting an identifier match.
 * Inputs may overlap each other; frames, work and matched must be disjoint from
 * each other and every input. Bad storage preserves work and output. */
TC_TLV_result TC_PIV_CMS_identifiers_match(const TC_PIV_CMS_object* object,
    TC_PIV_CMS_kind kind, TC_bytes fascn, TC_bytes uuid, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, int* matched);

#ifdef __cplusplus
}
#endif
#endif
