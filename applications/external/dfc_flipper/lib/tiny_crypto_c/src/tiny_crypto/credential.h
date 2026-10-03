/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Composed PIV and TWIC credential validation: signed CHUID, biometric
 * objects, the security object and PIV secure-messaging signer CVCs, bound to
 * an accepted CHUID and a shared validation context.
 * Standards: SP 800-73-5 Part 1, SP 800-76-2, FIPS 201-3, TWIC Part 2 v5.
 * Configuration: TC_ENABLE_CREDENTIAL, with CVC checks from
 * TC_ENABLE_PIV_CVC.
 * Limitations: card transport, cancellation status and the access decision
 * belong to the application. Iris records are unsupported.
 * Contracts: docs/api.md. Guides: docs/cms.md, docs/credential-validation.md. */
#ifndef TINY_CRYPTO_CREDENTIAL_H_
#define TINY_CRYPTO_CREDENTIAL_H_

#include <tiny_crypto/cms.h>
#include <tiny_crypto/lds.h>
#include <tiny_crypto/piv_card.h>
#include <tiny_crypto/piv_chuid.h>
#include <tiny_crypto/piv_cms.h>
#include <tiny_crypto/piv_security.h>
#include <tiny_crypto/validation.h>
#if TC_ENABLE_PIV_CVC
#include <tiny_crypto/piv_cvc.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_bytes encoded;
  TC_PIV_CHUID_encoding encoding;
  TC_PIV_card_profile profile;
  TC_PIV_CHUID_profile chuid_profile;
  /* Accept registered TWIC aliases and reader identifier rules for a PIV app.
   */
  int twic_reader_policy;
  /* Identifiers and expiration from the already validated card certificate. */
  const TC_PIV_card_identifiers* card;
  const TC_X509_time* card_expiration;
} TC_PIV_CHUID_validation_request;

/* Borrowed views of an accepted CHUID. The profile and time record the
 * validation that produced the result. Dependent validators take the FASC-N,
 * GUID and signer from it and reject a result from another profile or time.
 * revocation_checked is 1 when CRL evidence covered the signer path and 0
 * when TC_VALIDATION_REVOCATION_WHEN_AVAILABLE accepted it without. */
typedef struct {
  TC_PIV_CHUID object;
  TC_bytes signer;
  TC_X509_time at;
  TC_PIV_card_profile profile;
  uint8_t revocation_checked;
} TC_PIV_CHUID_report;

#if TC_ENABLE_CREDENTIAL
/* Authenticate a signed CHUID and bind it to the validated card certificate
 * (SP 800-73-5 Part 1 sections 3.1.2 and 3.1.2.1, TWIC Part 2 v5 section 6).
 * - request->profile selects the card profile. PIV takes the PIV or
 *   LEGACY_KEY_MAP CHUID profile. TWIC profiles take TWIC_SIGNED for the TWIC
 *   application, or LEGACY_KEY_MAP for the PIV application of a TWIC card,
 *   whose CHUID keeps the SP 800-73-2 Authentication Key Map (3D).
 *   twic_reader_policy is 0 or 1 and applies to the PIV profile only.
 * - card and card_expiration come from the already validated card
 *   certificate. The CHUID expiration date must be on or after the context
 *   time, and it includes the final second of its UTC date.
 * - The signer certificate needs a content-signing EKU for the profile and,
 *   for PIV, id-fpki-common-piv-contentSigning and a notAfter no earlier than
 *   card_expiration. Its path and CRL status use the context policies,
 *   including the revocation evidence policy.
 * - Request, card, context objects and their bytes stay stable and disjoint
 *   from the workspace, work and out. out borrows the CHUID and signer bytes.
 *
 * Work: one unit per storage comparison, the encoded length, then the CHUID,
 * CMS, identifier, signer policy, path and revocation steps.
 * Returns VALID with out written. REVOKED for a revoked signer path member.
 * ERROR for NULL or empty arguments, an unknown or mismatched profile value,
 * an incomplete context, a certificate purpose other than content signing
 * for the profile, or overlap, with work unchanged. LIMIT for an encoded
 * length above parsing.max_input or exhausted work or capacities. INVALID
 * for a malformed or expired CHUID, identifiers that differ from the card, a
 * signer outside the content-signer policy, or a failed signature or path.
 * UNAVAILABLE for a signer path member without CRL evidence under
 * TC_VALIDATION_REVOCATION_REQUIRED. UNSUPPORTED for unsupported algorithms
 * or CRLs. out changes only on VALID. */
TC_credential_status TC_PIV_CHUID_validate(const TC_PIV_CHUID_validation_request* request,
                                           const TC_validation_context* context, size_t* work,
                                           TC_PIV_CHUID_report* out);
#endif

typedef struct {
  /* Complete BC value, after any outer TWIC privacy-key decryption. */
  TC_bytes encoded;
  /* Must equal chuid->profile. */
  TC_PIV_card_profile profile;
  /* Accepted CHUID from TC_PIV_CHUID_validate at context->options->at. */
  const TC_PIV_CHUID_report* chuid;
  const TC_X509_time* card_expiration;
  /* Select the current or legacy biometric CMS profile explicitly. */
  TC_PIV_CMS_kind signature_profile;
  TC_PIV_CBEFF_format format;
  /* Set to one to require the CBEFF validity period at context time. */
  int require_current;
} TC_PIV_biometric_validation_request;

/* Borrowed views of an authenticated biometric object. record and
 * metadata.creator borrow request->encoded. signer borrows the embedded CMS
 * certificate or the CHUID signer. revocation_checked follows
 * TC_PIV_CHUID_report. */
typedef struct {
  TC_PIV_CBEFF_format format;
  TC_PIV_CBEFF_metadata metadata;
  TC_bytes record;
  TC_bytes signer;
  TC_PIV_card_profile profile;
  TC_X509_time at;
  uint8_t revocation_checked;
} TC_PIV_biometric_report;

#if TC_ENABLE_CREDENTIAL
/* Authenticate a biometric object's CBEFF header and record and bind the
 * FASC-N and GUID of the accepted CHUID (SP 800-76-2 sections 9.2 and 9.3).
 * An omitted CMS certificate selects the CHUID signer. An embedded
 * certificate must carry a different key (SP 800-76-2 section 9.3). The
 * CBEFF format must equal request->format. The record must pass
 * TC_PIV_fingerprint_read or TC_PIV_face_read under the card profile.
 * require_current checks the CBEFF validity period at the context time. The
 * signer follows the content-signer policy of TC_PIV_CHUID_validate.
 * Inputs stay borrowed, stable and disjoint from the workspace, work and out.
 *
 * Work: one unit per storage comparison, the encoded length, then the CMS,
 * identifier, signer, path and revocation steps.
 * Returns VALID with out written. ERROR for NULL or empty arguments, an
 * unknown profile, format or signature profile, an incomplete context, a
 * CHUID result that is incomplete or whose profile or time differs from the
 * request and context, or overlap, with work unchanged. UNSUPPORTED for iris
 * images, before any work, and for unsupported algorithms or CRLs.
 * UNAVAILABLE as in TC_PIV_CHUID_validate. LIMIT for an encoded length above
 * parsing.max_input or exhausted
 * work or capacities. INVALID for malformed or mismatched object data, a
 * record outside its profile, an expired period, a reused CHUID key or a
 * failed signature or path. REVOKED for a revoked signer path member. out
 * changes only on VALID. */
TC_credential_status TC_PIV_biometric_validate(const TC_PIV_biometric_validation_request* request,
                                               const TC_validation_context* context, size_t* work,
                                               TC_PIV_biometric_report* out);
#endif

/* One card object as hashed for the Security Object (SP 800-73-5 Part 1
 * section 3.1.7). parts supply its bytes in hash order. */
typedef struct {
  uint16_t container;
  const TC_bytes* parts;
  size_t count;
} TC_PIV_security_data;

typedef struct {
  /* Decoded LDS content scratch. Capacity is bounded by the application. */
  uint8_t* content;
  size_t content_capacity;
} TC_PIV_security_validation_workspace;

typedef struct {
  TC_bytes encoded;
  TC_PIV_security_encoding encoding;
  /* Must equal chuid->profile. */
  TC_PIV_card_profile profile;
  /* Accepted CHUID from TC_PIV_CHUID_validate at context->options->at. */
  const TC_PIV_CHUID_report* chuid;
  const TC_X509_time* card_expiration;
} TC_PIV_security_signature_request;

/* An authenticated Security Object: its container map, the signed LDS
 * digests and the parsing limits used for digest checks. object borrows the
 * request's encoded bytes. lds borrows those bytes or workspace->content.
 * Keep both stable and unchanged while the map is in use. The map survives
 * reuse of the validation workspace. revocation_checked follows
 * TC_PIV_CHUID_report. */
typedef struct {
  TC_PIV_security_object object;
  TC_LDS_security_object lds;
  TC_TLV_limits limits;
  TC_bytes signer;
  TC_PIV_card_profile profile;
  TC_X509_time at;
  uint8_t revocation_checked;
} TC_PIV_security_map;

#if TC_ENABLE_CREDENTIAL
/* Authenticate a Security Object with the accepted CHUID signer and decode
 * its signed LDS digests (SP 800-73-5 Part 1 section 3.1.7). The container
 * map must name exactly the signed data groups. Objects are checked
 * separately with TC_PIV_security_digest_check, so a partial inventory, such
 * as one read without the PIN, can still be checked container by container.
 * - workspace->content receives the decoded LDS content. Size it for the
 *   largest LDSSecurityObject the application accepts. It must be disjoint
 *   from every input.
 * - Request, CHUID, context objects and their bytes stay stable and disjoint
 *   from the workspace, work and out.
 *
 * Work: one unit per storage comparison, the encoded length, then the CMS,
 * signer, path, revocation and LDS decoding steps.
 * Returns VALID with out written. ERROR for NULL or empty arguments, an
 * unknown encoding or profile, an incomplete context, a CHUID result whose
 * profile or time differs, or overlap, with work unchanged. LIMIT for an
 * encoded length above parsing.max_input or exhausted work or capacities.
 * INVALID for malformed data, a failed signature or path, or a container map
 * whose groups differ from the signed LDS groups. REVOKED, UNAVAILABLE and
 * UNSUPPORTED come from the signer path and revocation checks as in
 * TC_PIV_CHUID_validate. out changes only on VALID. */
TC_credential_status TC_PIV_security_authenticate(
    const TC_PIV_security_signature_request* request, const TC_validation_context* context,
    const TC_PIV_security_validation_workspace* workspace, size_t* work, TC_PIV_security_map* out);

/* Check one object against the signed digest of its container in an
 * authenticated map. object->parts supply the bytes in hash order, usually
 * the value of the GET DATA 53 container. The map, the object, its parts and
 * their bytes stay stable and disjoint from work. Parse frames and hash
 * scratch live on the stack. The hash scratch is wiped on return.
 *
 * Work: one unit per storage comparison, the map lookup, the digest scan and
 * the hashed bytes.
 * Returns VALID for a matching digest. INVALID for a different digest.
 * UNAVAILABLE when the signed map does not name object->container. ERROR for
 * NULL arguments, an object without parts, a map whose group sets differ, or
 * overlap, with work unchanged. LIMIT for more parts than
 * map->limits.max_elements or exhausted work. UNSUPPORTED for a digest
 * algorithm this build disables. */
TC_credential_status TC_PIV_security_digest_check(const TC_PIV_security_map* map,
                                                  const TC_PIV_security_data* object, size_t* work);
#endif

typedef struct {
  TC_bytes encoded;
  TC_PIV_security_encoding encoding;
  /* Must equal chuid->profile. */
  TC_PIV_card_profile profile;
  /* Accepted CHUID from TC_PIV_CHUID_validate at context->options->at. */
  const TC_PIV_CHUID_report* chuid;
  const TC_X509_time* card_expiration;
  /* Complete inventory. Container IDs must be unique, and each object has parts. */
  const TC_PIV_security_data* objects;
  size_t count;
} TC_PIV_security_validation_request;

/* Inventory descriptors and their bytes remain borrowed and immutable through
 * subsequent checks. This result survives reuse of the validation workspace.
 * revocation_checked follows TC_PIV_CHUID_report. */
typedef struct {
  const TC_PIV_security_data* objects;
  size_t count;
  TC_bytes signer;
  TC_PIV_card_profile profile;
  TC_X509_time at;
  uint8_t revocation_checked;
} TC_PIV_security_report;

#if TC_ENABLE_CREDENTIAL
/* Authenticate a Security Object with TC_PIV_security_authenticate, then
 * check the exact inventory against its signed LDS digests with
 * TC_PIV_security_digest_check (SP 800-73-5 Part 1 section 3.1.7). Every
 * signed data group must appear once in the inventory.
 * - workspace->content receives the decoded LDS content. Size it for the
 *   largest LDSSecurityObject the application accepts. It must be disjoint
 *   from every input.
 * - All buffers stay caller-owned, stable and disjoint from the workspace,
 *   work and out. out borrows the inventory and signer bytes.
 *
 * Work: one unit per storage comparison, the encoded length, then the CMS,
 * signer, path, revocation, LDS decoding and digest steps.
 * Returns VALID with out written. LIMIT for more than TC_LDS_MAX_GROUPS
 * objects before any other check, and for an encoded length above
 * parsing.max_input or exhausted work or capacities. ERROR for NULL or empty
 * arguments, an unknown encoding or profile, empty parts, an incomplete
 * context, a CHUID result whose profile or time differs, or overlap. INVALID
 * for fewer than two objects or duplicate containers, before any work, and
 * for malformed data or an inventory that differs from the signed digests.
 * REVOKED, UNAVAILABLE and UNSUPPORTED come from the signer path and
 * revocation checks. out changes only on VALID. */
TC_credential_status TC_PIV_security_validate(const TC_PIV_security_validation_request* request,
                                              const TC_validation_context* context,
                                              const TC_PIV_security_validation_workspace* workspace,
                                              size_t* work, TC_PIV_security_report* out);
#endif

enum { TC_TWIC_UNSIGNED_CHUID_CONTAINER = 0x3002 };

typedef struct {
  TC_bytes encoded;
  TC_PIV_CHUID_encoding encoding;
  /* Must equal security->profile. */
  TC_PIV_card_profile profile;
  const TC_PIV_card_identifiers* card;
  /* Accepted inventory from TC_PIV_security_validate at context->options->at. */
  const TC_PIV_security_report* security;
} TC_TWIC_unsigned_CHUID_validation_request;

#if TC_ENABLE_CREDENTIAL
/* Check an unsigned TWIC CHUID against an inventory accepted by
 * TC_PIV_security_validate at the same evaluation time. Container 3002 of the
 * inventory must match encoded byte for byte. The CHUID must be current and
 * its identifiers must match card under TWIC reader rules. The inventory and
 * inputs stay borrowed and stable, and work is disjoint from them.
 *
 * Work: one unit per storage comparison, then the compared inventory bytes
 * and the identifier comparison.
 * Returns VALID for a matching, current CHUID. ERROR for NULL or empty
 * arguments, a non-TWIC profile, an empty or oversized inventory, a security
 * result whose profile or time differs, or overlap, with work unchanged.
 * LIMIT for an encoded length above parsing.max_input or exhausted work.
 * INVALID for a missing or different container 3002, a malformed or expired
 * CHUID, or other card identifiers. */
TC_credential_status
TC_TWIC_unsigned_CHUID_validate(const TC_TWIC_unsigned_CHUID_validation_request* request,
                                const TC_validation_context* context, size_t* work);
#endif

#if TC_ENABLE_CREDENTIAL
/* Validate a content signer certificate, such as the secure-messaging
 * Certificate Signer of container 5FC122 (SP 800-73-5 Part 1 section 3.3.7),
 * under the content-signer policy of TC_PIV_CHUID_validate for profile: a
 * content-signing EKU for the profile, digitalSignature, and for PIV
 * id-fpki-common-piv-contentSigning. TC_X509_validate then checks the path
 * and revocation under the context. certificate must be DER, stable and
 * disjoint from work and out. out->certificate borrows certificate.
 *
 * Work: one unit per storage comparison, the certificate and its policy
 * scan, then the path and revocation steps.
 * Returns VALID with out written, including revocation_checked. ERROR for
 * NULL or empty arguments, an unknown profile, a certificate purpose other
 * than content signing, an incomplete context or overlap, with work
 * unchanged. INVALID for a signer outside the policy or a failed path.
 * REVOKED, UNAVAILABLE, UNSUPPORTED and LIMIT follow TC_X509_validate. out
 * changes only on VALID. */
TC_credential_status TC_PIV_content_signer_validate(TC_bytes certificate,
                                                    TC_PIV_card_profile profile,
                                                    const TC_validation_context* context,
                                                    size_t* work, TC_X509_validation_report* out);
#endif

#if TC_ENABLE_PIV_CVC
typedef struct {
  TC_bytes card, intermediate, expected_uuid, signer_certificate;
  TC_EC_curve curve;
  TC_PIV_card_profile profile;
} TC_PIV_CVC_validation_request;

#if TC_ENABLE_CREDENTIAL
/* Validate the X.509 signer of a PIV secure-messaging CVC chain with
 * TC_PIV_content_signer_validate, then verify the chain with
 * TC_PIV_CVC_chain_verify (SP 800-73-5 Part 1 section 3.3.7, Part 2 section
 * 4.1.5). The card profile selects the accepted content-signing OIDs. point
 * is EC scratch, cleared after each point check.
 * Inputs stay borrowed, stable and disjoint from point, the workspace, work
 * and out. out borrows request->card.
 *
 * Work: one unit per storage comparison, the signer certificate and its
 * policy scan, the signer path and revocation, then the chain.
 * Returns VALID with out written. ERROR for NULL or empty arguments, an
 * unknown profile, a certificate purpose other than content signing, an
 * incomplete context or overlap, with work unchanged. The signer statuses
 * follow TC_X509_validate, including the revocation evidence policy.
 * TC_PIV_CVC holds no revocation flag. Under
 * TC_VALIDATION_REVOCATION_WHEN_AVAILABLE, read revocation_checked from
 * TC_X509_validate on the signer. The chain statuses follow TC_PIV_CVC_chain_verify, mapped
 * to the credential status of the same name. out changes only on VALID.
 * Secure messaging also requires key confirmation. */
TC_credential_status TC_PIV_CVC_validate(const TC_PIV_CVC_validation_request* request,
                                         const TC_validation_context* context,
                                         TC_EC_workspace* point, size_t* work, TC_PIV_CVC* out);
#endif
#endif

#ifdef __cplusplus
}
#endif
#endif
