/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Composed card check: every check of a PIV or TWIC card inventory as a
 * report entry that PASSED, FAILED or is NOT_CHECKABLE with a reason, live
 * key proofs, and an acceptance test against the application's
 * requirements.
 * Standards: NIST SP 800-73-5 Part 1 sections 3.1, 3.3 and 4.1.1, Tables 2
 * and 8, Part 2 section 4.1.5, SP 800-76-2 section 9, FIPS 201-3, RFC 5280
 * section 6, RFC 6960, TWIC Part 2 v5 sections 4.2, 4.5 and 6.
 * Configuration: TC_ENABLE_PIV_CARD_CHECK (requires TC_ENABLE_CREDENTIAL,
 * TC_ENABLE_PIV_CATALOG, TC_ENABLE_X509_PATH, TC_ENABLE_X509_REVOCATION and
 * TC_ENABLE_GZIP). OCSP evidence needs TC_ENABLE_X509_OCSP, key proofs
 * TC_ENABLE_PIV_KEY_PROOF and the SM_CVC check TC_ENABLE_PIV_CVC.
 * Limitations: iris records, TWIC Privacy Key decryption and the TWIC
 * cancelled card list belong to the application. A report states what was
 * checked. The access decision belongs to the application, which states its
 * requirements to TC_PIV_card_report_accepts.
 * Contracts: docs/api.md, including its size_t work units.
 * Guide: docs/piv-card-check.md. */
#ifndef TINY_CRYPTO_PIV_CARD_CHECK_H_
#define TINY_CRYPTO_PIV_CARD_CHECK_H_
#include <tiny_crypto/credential.h>
#include <tiny_crypto/ec.h>
#include <tiny_crypto/piv_catalog.h>
#include <tiny_crypto/piv_certificate.h>
#include <tiny_crypto/x509_crl.h>
#if TC_ENABLE_PIV_KEY_PROOF
#include <tiny_crypto/piv_key_proof.h>
#endif
#if TC_ENABLE_PIV_CARD_CHECK
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  TC_PIV_CHECK_PASSED,
  TC_PIV_CHECK_FAILED,
  TC_PIV_CHECK_NOT_CHECKABLE
} TC_PIV_check_outcome;

/* What one report entry checked.
 * MANDATORY_OBJECT          a mandatory catalog object is present (Part 1
 *                           Table 2 M rows, TWIC Part 2 v5 4.5 Y rows).
 * CERTIFICATE_PATH          a card certificate decodes and has a valid path
 *                           for its key (RFC 5280 section 6.1).
 * REVOCATION                OCSP or CRL evidence covers a certificate or a
 *                           signer path (RFC 5280 6.3, RFC 6960). container
 *                           names the certificate or the signed object.
 * CERTIFICATE_IDENTIFIERS   the FASC-N and UUIDs of a card certificate
 *                           (Part 1 sections 3.1.3, 3.1.4 and 3.4).
 * CHUID                     the signed CHUID, its signer and expiration
 *                           (Part 1 section 3.1.2).
 * SECURITY_SIGNATURE        the Security Object signature and its map
 *                           (Part 1 section 3.1.7).
 * SECURITY_DIGEST           one container against its signed digest.
 * BIOMETRIC                 a signed biometric object (SP 800-76-2 9.2).
 * PRINTED_EXPIRATION        the printed expiration date matches the CHUID
 *                           and is current (Part 1 section 3.3.1).
 * DISCOVERY_CONSISTENCY     a nonempty BIT group comes with the OCC bit of
 *                           the Discovery Object (Part 1 section 3.3.6).
 * SM_SIGNER                 the secure messaging Certificate Signer of
 *                           5FC122 as a content signer (section 3.3.7).
 * SM_CVC                    the card CVC of the session under that signer
 *                           (Part 2 section 4.1.5).
 * KEY_PROOF                 a card key signed a fresh challenge.
 * COPY_MATCH                an object read plain equals the inventory copy
 *                           byte for byte. */
typedef enum {
  TC_PIV_CHECK_MANDATORY_OBJECT,
  TC_PIV_CHECK_CERTIFICATE_PATH,
  TC_PIV_CHECK_REVOCATION,
  TC_PIV_CHECK_CERTIFICATE_IDENTIFIERS,
  TC_PIV_CHECK_CHUID,
  TC_PIV_CHECK_SECURITY_SIGNATURE,
  TC_PIV_CHECK_SECURITY_DIGEST,
  TC_PIV_CHECK_BIOMETRIC,
  TC_PIV_CHECK_PRINTED_EXPIRATION,
  TC_PIV_CHECK_DISCOVERY_CONSISTENCY,
  TC_PIV_CHECK_SM_SIGNER,
  TC_PIV_CHECK_SM_CVC,
  TC_PIV_CHECK_KEY_PROOF,
  TC_PIV_CHECK_COPY_MATCH
} TC_PIV_check_kind;

/* Why an entry FAILED or is NOT_CHECKABLE. NONE for PASSED and for a
 * failed validation, whose status says why.
 * ABSENT         the card reported the object missing.
 * EMPTY          the object is empty (53 00, Part 1 section 4.1.1).
 * RESTRICTED     the access rule was unmet, so nothing was read or sent,
 *                or the link was not secured for an SM_CVC check.
 * DENIED         the card refused the read.
 * NO_EVIDENCE    no current OCSP or CRL evidence covers a path member.
 * UNSUPPORTED    an algorithm, format or object this build cannot check.
 * DEPENDENCY     a prerequisite check did not pass.
 * NOT_REQUESTED  the input was not supplied or the inventory skipped it.
 * LIMIT          a capacity, parsing limit or work budget ran out.
 * CARD_STATUS    the card answered a key proof with card_status.
 * OVERSIZED      the object did not fit the inventory pool. */
typedef enum {
  TC_PIV_REASON_NONE,
  TC_PIV_REASON_ABSENT,
  TC_PIV_REASON_EMPTY,
  TC_PIV_REASON_RESTRICTED,
  TC_PIV_REASON_DENIED,
  TC_PIV_REASON_NO_EVIDENCE,
  TC_PIV_REASON_UNSUPPORTED,
  TC_PIV_REASON_DEPENDENCY,
  TC_PIV_REASON_NOT_REQUESTED,
  TC_PIV_REASON_LIMIT,
  TC_PIV_REASON_CARD_STATUS,
  TC_PIV_REASON_OVERSIZED
} TC_PIV_check_reason;

/* One report entry.
 * status         the validator status behind the outcome: VALID for PASSED,
 *                INVALID or REVOKED for FAILED, and UNAVAILABLE,
 *                UNSUPPORTED or LIMIT for NOT_CHECKABLE. Checks that run no
 *                validator record VALID, INVALID or UNAVAILABLE.
 * container      the container ID of the object checked (Part 1 Table 8),
 *                0 when the entry names none.
 * card_status    the card status word for ABSENT, DENIED and CARD_STATUS,
 *                else 0.
 * kind, outcome, reason
 *                TC_PIV_check_kind, TC_PIV_check_outcome and
 *                TC_PIV_check_reason values.
 * key_reference  the key of a certificate or key proof, else 0. */
typedef struct {
  TC_credential_status status;
  uint16_t container, card_status;
  uint8_t kind, outcome, reason, key_reference;
} TC_PIV_check;

/* Report capacity. A full card currently needs 40 entries. */
#define TC_PIV_CARD_CHECKS_MAX 96u

/* Certificate slots of TC_PIV_card_report and TC_PIV_card_check_ocsp. */
enum {
  TC_PIV_CARD_SLOT_PIV_AUTHENTICATION,  /* 9A, container 0101 */
  TC_PIV_CARD_SLOT_DIGITAL_SIGNATURE,   /* 9C, container 0100 */
  TC_PIV_CARD_SLOT_KEY_MANAGEMENT,      /* 9D, container 0102 */
  TC_PIV_CARD_SLOT_CARD_AUTHENTICATION, /* 9E, container 0500 */
  TC_PIV_CARD_CERTIFICATES
};

/* A complete report.
 * checks, count     the entries in check order.
 * application,
 * profile, at       the inventory application, the credential profile and
 *                   the evaluation time.
 * card, card_expiration
 *                   identifiers and notAfter of the Card Authentication
 *                   certificate when has_card is 1.
 * chuid             the accepted CHUID when has_chuid is 1.
 * security          the authenticated Security Object when has_security is 1.
 * certificates      the parsed card certificates by slot.
 *                   certificate_valid[slot] is 1 when the path is valid and
 *                   the revocation policy of the card context accepts its
 *                   evidence. Only those certificates take key proofs.
 * Views borrow the inventory pool, the request's certificate and LDS
 * workspace buffers and the trust inputs. Keep them unchanged while using
 * the report. */
typedef struct {
  TC_PIV_check checks[TC_PIV_CARD_CHECKS_MAX];
  size_t count;
  TC_PIV_card_identifiers card;
  TC_X509_time card_expiration, at;
  TC_PIV_CHUID_report chuid;
  TC_PIV_security_map security;
  TC_X509_certificate certificates[TC_PIV_CARD_CERTIFICATES];
  TC_PIV_card_profile profile;
  TC_PIV_application_id application;
  uint8_t has_card, has_chuid, has_security;
  uint8_t certificate_valid[TC_PIV_CARD_CERTIFICATES];
} TC_PIV_card_report;

/* OCSP evidence for the card certificates: one DER OCSPResponse per slot,
 * or an empty span. Each response covers its end-entity certificate and is
 * verified against the issuer and time of the card context
 * (TC_X509_path_check_revocation). max_responses and max_certificates bound
 * each response as in TC_X509_revocation_ocsp. The other path members use
 * the CRLs of the card context. */
typedef struct {
  TC_bytes responses[TC_PIV_CARD_CERTIFICATES];
  size_t max_responses, max_certificates;
} TC_PIV_card_check_ocsp;

/* Inputs of one card check. Everything is borrowed for the call and, for the
 * spans that the report keeps, while the report is in use.
 * inventory        a read inventory. Its link member gives the interface,
 *                  application and access state of the reads.
 * link             the live link that read it, or NULL for retained
 *                  objects. SM_CVC needs a secured link.
 * profile          the credential profile. On the TWIC application it equals
 *                  the inventory profile. On the PIV application it is
 *                  TC_PIV_CARD, or a TWIC profile for the PIV application of
 *                  a TWIC card.
 * card             trust, CRLs, time and revocation policy for the card
 *                  certificates.
 * content          trust, CRLs, time and revocation policy for the CHUID,
 *                  Security Object, biometric and secure messaging signers.
 *                  Both contexts use the same evaluation time and may share
 *                  one arena.
 * ocsp             OCSP evidence for the card certificates, or NULL.
 * sm_card_cvc      the card CVC of the secure messaging session (the
 *                  certificate of TC_PIV_SM_key_request's peer), or empty.
 * plain_copies     objects read before secure messaging, such as 5FC122,
 *                  for COPY_MATCH. NULL with a zero count for none. */
typedef struct {
  const TC_PIV_inventory* inventory;
  const TC_PIV_link* link;
  TC_PIV_card_profile profile;
  const TC_validation_context* card;
  const TC_validation_context* content;
  const TC_PIV_card_check_ocsp* ocsp;
  TC_bytes sm_card_cvc;
  const TC_PIV_object* plain_copies;
  size_t plain_copy_count;
} TC_PIV_card_check_request;

/* Select the CHUID encoding rules for an application and credential profile.
 * The PIV application on a TWIC card uses the legacy key-map form; the TWIC
 * application uses its signed form. Invalid combinations return ARGUMENT. */
TC_TLV_result TC_PIV_card_chuid_profile(TC_PIV_application_id application,
                                        TC_PIV_card_profile profile, TC_PIV_CHUID_profile* out);

/* Scratch and output storage of one card check.
 * gzip          GZIP decoder scratch for compressed certificates.
 * point         EC scratch for CVC point checks, cleared after each check.
 * certificates  receives decoded GZIP certificates, one after another. The
 *               report keeps views of them. 16 KiB holds five certificates
 *               of the SD 33 cards.
 * lds_content   receives the decoded LDS content of the Security Object
 *               (TC_PIV_security_authenticate). 4 KiB covers 16 groups. */
typedef struct {
  TC_GZIP_workspace gzip;
  TC_EC_workspace point;
  TC_buffer certificates;
  TC_buffer lds_content;
} TC_PIV_card_check_workspace;

/* Run every check of the inventory and write the report. The checks, in
 * report order:
 * - MANDATORY_OBJECT for each mandatory catalog entry: PRESENT passes.
 *   ABSENT, DENIED and EMPTY fail. RESTRICTED, OVERSIZED and SKIPPED are
 *   NOT_CHECKABLE. On the PIV application of a TWIC card a contactless
 *   denial is NOT_CHECKABLE/DENIED, since TWIC Part 2 v5 4.2 makes its
 *   objects Never on contactless.
 * - Card certificates 9E, then the CHUID, then 9A, 9C and 9D:
 *   TC_PIV_certificate_decode, CERTIFICATE_PATH under the card context with
 *   the key's purpose and REVOCATION from the slot's OCSP response or the
 *   CRLs. 9E follows the purpose rules of TC_PIV_card_certificate_validate
 *   (TWIC Part 2 v5 section 6). 9A and 9C need digitalSignature. 9D takes
 *   the card context policy. The path check accepts missing evidence, and
 *   REVOCATION reports it as NO_EVIDENCE. CERTIFICATE_IDENTIFIERS reads the
 *   9E subjectAltName, and the 9A one against the CHUID GUID and FASC-N.
 * - CHUID with TC_PIV_CHUID_validate under the content context and
 *   REVOCATION 3000 from its revocation_checked.
 * - SECURITY_SIGNATURE with TC_PIV_security_authenticate, REVOCATION 9000,
 *   and SECURITY_DIGEST for every mapped container. The map proves the
 *   object exists, so ABSENT, DENIED and EMPTY fail. The digest covers the
 *   value of the 53 container, or of 7E for the Discovery Object. On the
 *   TWIC application the 3001 digest covers the plaintext printed
 *   information (TWIC Part 2 v5 4.6.5 note 1), which the card stores TPK
 *   encrypted, so it is NOT_CHECKABLE/UNSUPPORTED.
 * - BIOMETRIC for fingerprints and the facial image with
 *   TC_PIV_biometric_validate, and NOT_CHECKABLE/UNSUPPORTED for iris and
 *   the TWIC Privacy Key encrypted objects of the TWIC application.
 * - PRINTED_EXPIRATION after a passed digest of 3001.
 * - DISCOVERY_CONSISTENCY when the catalog holds the Discovery Object and
 *   the BIT group. Both get their integrity from the Security Object, so
 *   the check needs an authenticated map and a passed digest of each of
 *   them that the map names (Part 1 sections 3.3.2 and 3.3.6).
 * - SM_SIGNER with TC_PIV_content_signer_validate on 5FC122, REVOCATION
 *   1017, and SM_CVC with TC_PIV_CVC_chain_verify of sm_card_cvc under that
 *   signer, the 5FC122 intermediate CVC and the CHUID GUID. SM_CVC passes
 *   only on a link that reports secured without a lost session, with the
 *   curve of its suite (27 P-256, 2E P-384). Key confirmation happened when
 *   the session was established.
 * - COPY_MATCH for each plain copy against the inventory entry of the same
 *   container.
 * A check whose prerequisite did not pass is NOT_CHECKABLE/DEPENDENCY. The
 * validator statuses map to outcomes as TC_PIV_check.status describes. A
 * validator ERROR means a broken contract and aborts.
 *
 * Only a PASSED entry is success. TC_PIV_OK means the report is complete and
 * accepts nothing by itself. Decide with TC_PIV_card_report_accepts.
 *
 * Work: the charges of each validator and of GZIP decoding. Exhausted work
 * makes the remaining checks NOT_CHECKABLE/LIMIT.
 *
 * TC_PIV_ARGUMENT  NULL request, inventory, contexts, workspace, work or out,
 *                  an inventory without entries, entries without catalog
 *                  info or a count above capacity, contexts without options
 *                  or workspace, different evaluation times, an unknown
 *                  profile or one that does not fit the inventory
 *                  application, plain copies NULL with a count or without
 *                  info, workspace buffers NULL with a capacity, or out or
 *                  *work overlapping the request, the inventory, its objects
 *                  or pool, the link, the plain copies and their bytes, the
 *                  OCSP responses, the card CVC or the workspace.
 * TC_PIV_LIMIT     more than TC_PIV_CARD_CHECKS_MAX entries.
 * TC_PIV_ERROR     a validator reported a broken contract.
 * An argument error leaves out, *work and the workspace unchanged. LIMIT and
 * ERROR wipe out. */
TC_PIV_result TC_PIV_card_check(const TC_PIV_card_check_request* request,
                                TC_PIV_card_check_workspace* workspace, size_t* work,
                                TC_PIV_card_report* out);

/* A requirement on a report: kind, and the key reference and container when
 * nonzero. */
typedef struct {
  uint8_t kind, key_reference;
  uint16_t container;
} TC_PIV_check_requirement;

/* The first entry of report that matches requirement, or NULL. NULL
 * arguments return NULL. */
const TC_PIV_check* TC_PIV_card_report_find(const TC_PIV_card_report* report,
                                            const TC_PIV_check_requirement* requirement);

/* 1 when every requirement matches at least one entry and every matching
 * entry PASSED, else 0. NOT_CHECKABLE never passes. A NULL report, NULL
 * required or a zero count returns 0. */
int TC_PIV_card_report_accepts(const TC_PIV_card_report* report,
                               const TC_PIV_check_requirement* required, size_t count);

/* Scratch of TC_PIV_card_crl_targets.
 * gzip          GZIP decoder scratch for compressed certificates.
 * certificates  receives decoded GZIP certificates, one after another. The
 *               targets of those certificates borrow it.
 * parsing       frames and extension OID slots for one certificate or CMS
 *               object. */
typedef struct {
  TC_GZIP_workspace gzip;
  TC_buffer certificates;
  TC_X509_workspace parsing;
} TC_PIV_crl_target_workspace;

/* List the CRL lookup targets of a read inventory: the issuer and serial of
 * each certificate whose revocation TC_PIV_card_check queries (RFC 5280
 * section 5.3.3). Prepare a source-backed CRL for these targets with
 * TC_X509_crl_prepare_begin, so that a large CRL is scanned once and only
 * their entries are kept.
 * - The certificates are those of the present certificate containers,
 *   including the secure messaging signer, and every certificate embedded in
 *   the CMS of the CHUID, the Security Object and the biometric objects.
 *   A pair listed once is not repeated.
 * - Targets are collected before authentication. The card check
 *   authenticates every certificate it relies on. An object that does not
 *   decode adds no target, so a lookup for its certificate returns
 *   UNSUPPORTED and its revocation check is not satisfied.
 * - Targets borrow the inventory pool and workspace->certificates. Keep them
 *   unchanged while the targets are used.
 * limits bound each certificate and CMS object. inventory, its objects and
 * pool, limits, the workspace, work, targets and count must be disjoint.
 *
 * Work: the encoded bytes of each certificate and CMS read, and the GZIP
 * output of each compressed certificate.
 * Returns TC_PIV_OK with *count written. TC_PIV_ARGUMENT for NULL arguments,
 * targets NULL with a capacity, workspace buffers NULL with a capacity or
 * overlap, with every output unchanged. TC_PIV_LIMIT for more targets than
 * capacity, a full workspace->certificates, limits too small for an object or
 * exhausted work, with *count unchanged. The workspace, targets and work are
 * provisional on failure. */
TC_PIV_result TC_PIV_card_crl_targets(const TC_PIV_inventory* inventory,
                                      const TC_TLV_limits* limits,
                                      TC_PIV_crl_target_workspace* workspace, size_t* work,
                                      TC_X509_crl_target* targets, size_t capacity, size_t* count);

/* Certificate check of one retained card certificate.
 * encoded             the DER certificate.
 * profile             the credential profile.
 * key_reference       9E (Card Authentication) or 9A (PIV Authentication,
 *                     TC_PIV_CARD only).
 * twic_reader_policy  1 applies TWIC reader identifier rules to 9A (TWIC
 *                     Part 3 v4 4.4.4), else 0.
 * card_guid           for 9A, the 16-byte GUID of the card's CHUID. */
typedef struct {
  TC_bytes encoded;
  TC_PIV_card_profile profile;
  uint8_t key_reference;
  uint8_t twic_reader_policy;
  TC_bytes card_guid;
} TC_PIV_card_certificate_request;

typedef struct {
  TC_X509_validation_report certificate;
  TC_PIV_card_identifiers identifiers;
} TC_PIV_card_certificate_report;

/* Validate a card certificate under context with the purpose of its key and
 * read its identifiers (SP 800-73-5 Part 1 sections 3.1.3, 3.1.4 and 3.4,
 * TWIC Part 2 v5 section 6).
 * - 9E: context->options->certificate.purpose is empty or a card
 *   authentication OID. The PIV profile takes it, or the one PIV card
 *   authentication EKU of the certificate. TWIC profiles take the one PIV or
 *   TWIC card authentication EKU. The identifiers follow
 *   TC_PIV_card_identifiers_read, or TC_TWIC_card_identifiers_read for TWIC.
 *   The path requires digitalSignature and that EKU, and inhibits
 *   anyExtendedKeyUsage.
 * - 9A: the path requires digitalSignature and no EKU purpose. The
 *   identifiers follow TC_PIV_authentication_identifiers_read or, with
 *   twic_reader_policy, TC_TWIC_authentication_identifiers_read against
 *   card_guid.
 * - Path and revocation follow TC_X509_validate.
 * The request and its bytes stay stable and disjoint from work and out.
 * out borrows encoded.
 *
 * Work: twice the certificate length for the purpose scan, the extension
 * bytes, then the TC_X509_validate and identifier charges.
 * Returns VALID with out written. ERROR for NULL arguments, an empty
 * certificate, another key reference, 9A on a TWIC profile, a
 * twic_reader_policy other than 0 or 1 or with 9E, a card_guid without 16
 * bytes for 9A, a context purpose other than card authentication, or an
 * incomplete context. INVALID for a malformed certificate, no or several
 * card authentication EKUs on 9E, a failed path or bad identifiers.
 * REVOKED, UNAVAILABLE, UNSUPPORTED and LIMIT follow TC_X509_validate and
 * the identifier readers. out changes only on VALID. */
TC_credential_status
TC_PIV_card_certificate_validate(const TC_PIV_card_certificate_request* request,
                                 const TC_validation_context* context, size_t* work,
                                 TC_PIV_card_certificate_report* out);

#if TC_ENABLE_PIV_KEY_PROOF
/* TC_PIV_card_proof_request.keys bits. */
enum {
  TC_PIV_CARD_PROVE_DIGITAL_SIGNATURE = 1u << 0,
  TC_PIV_CARD_PROVE_PIV_AUTHENTICATION = 1u << 1,
  TC_PIV_CARD_PROVE_CARD_AUTHENTICATION = 1u << 2
};

/* Key proofs to add to a report.
 * keys      TC_PIV_CARD_PROVE_* bits, at least one.
 * policy    the key policy. Its profile equals the report profile.
 * random    the challenge source.
 * provider  the signature provider. */
typedef struct {
  unsigned keys;
  TC_PIV_key_policy policy;
  TC_random_source random;
  const TC_X509_signature_provider* provider;
} TC_PIV_card_proof_request;

/* Prove the requested keys with TC_PIV_key_prove and append one KEY_PROOF
 * entry per key, in the order 9C, 9A, 9E. 9C runs first so that it directly
 * follows the caller's PIN submission (PIN Always, Part 1 Table 5). Each
 * proof uses report->certificates[slot] and needs certificate_valid, else
 * the entry is NOT_CHECKABLE/DEPENDENCY and nothing is sent.
 * - OK passes. INVALID fails.
 * - REFUSED is NOT_CHECKABLE/RESTRICTED, CARD_STATUS is
 *   NOT_CHECKABLE/CARD_STATUS with the card status, UNSUPPORTED and LIMIT
 *   are NOT_CHECKABLE with that reason.
 * - ERROR from the transport, provider or random source aborts.
 * The report must come from TC_PIV_card_check on the same link, and it
 * keeps its borrowed views.
 *
 * Work: the charges of TC_PIV_key_prove.
 * TC_PIV_ARGUMENT  NULL arguments, keys zero or with unknown bits, 9A or 9C
 *                  with a report of the TWIC application, a policy profile
 *                  other than the report's, or the report, request or work
 *                  overlapping each other or the workspace. Nothing changes.
 * TC_PIV_LIMIT     the report is full. The report is wiped.
 * TC_PIV_ERROR     a proof returned ERROR, or ARGUMENT for inputs that
 *                  TC_PIV_key_prove rejects, such as report certificates
 *                  inside the link storage. The report is wiped.
 * Every return wipes the workspace except ARGUMENT. */
TC_PIV_result TC_PIV_card_prove_keys(TC_PIV_link* link, const TC_PIV_card_proof_request* request,
                                     TC_PIV_key_proof_workspace* workspace, TC_work_budget* work,
                                     TC_PIV_card_report* report);
#endif

#ifdef __cplusplus
}
#endif
#endif
#endif
