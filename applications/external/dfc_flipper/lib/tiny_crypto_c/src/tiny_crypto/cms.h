/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* CMS SignedData parsing and signer verification: envelopes, SignerInfo,
 * signed attributes, content binding and the SignerInfo signature.
 * Standards: RFC 5652, RFC 3370, RFC 5035, RFC 6211.
 * Configuration: TC_ENABLE_CMS, with BER envelopes from TC_TLV_ENABLE_BER.
 * Limitations: unsigned attributes, including countersignatures, are
 * unauthenticated and ignored. Signer paths and revocation are in
 * cms_validation.h.
 * Contracts: docs/api.md. Guide: docs/cms.md. */
#ifndef TINY_CRYPTO_CMS_H_
#define TINY_CRYPTO_CMS_H_
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  /* RFC 5652 section 1.2: the envelope may use any BER encoding. */
  TC_CMS_ENVELOPE_BER = 0,
  /* Restrict envelope framing to DER: minimal definite lengths and primitive
   * OCTET STRING values. Signed attributes follow their own encoding. */
  TC_CMS_ENVELOPE_DER = 1
} TC_CMS_envelope_encoding;

typedef enum {
  /* RFC 5652 signed attributes, including DER SET OF ordering. */
  TC_CMS_ATTRIBUTES_DER = 0,
  /* Application opt-in. Preserves definite length octets and member order. */
  TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER = 1
} TC_CMS_attribute_encoding;

typedef enum {
  TC_CMS_RSA_PARAMETERS_NULL = 0,
  /* Accept an omitted rsaEncryption signatureAlgorithm parameter. */
  TC_CMS_RSA_PARAMETERS_ALLOW_ABSENT = 1
} TC_CMS_rsa_parameters;

/* Signed-attribute identifiers the reader interprets. */
typedef enum {
  /* contentType, messageDigest, signingTime (RFC 5652 section 11),
   * SMIMECapabilities (RFC 8551 section 2.5.2) and entryUUID (RFC 4530). */
  TC_CMS_ATTRIBUTE_OIDS_CMS = 0,
  /* CMS identifiers plus pivSigner-DN and pivFASC-N (FIPS 201-3 Table B-2).
   * twicFASC-N returns UNSUPPORTED under every other-attribute policy. */
  TC_CMS_ATTRIBUTE_OIDS_PIV = 1,
  /* PIV identifiers plus twicFASC-N (TWIC Part 2 v5 section 6). */
  TC_CMS_ATTRIBUTE_OIDS_PIV_TWIC = 2
} TC_CMS_attribute_oids;

/* Handling of signed attributes outside the selected identifiers. Skipped
 * attributes stay covered by the signature and are left to the application.
 * A repeated attribute type returns INVALID. */
typedef enum {
  /* Skip cmsAlgorithmProtection (RFC 6211), signingCertificate and
   * signingCertificateV2 (RFC 5035). Each needs one SEQUENCE value. Other
   * attributes return UNSUPPORTED. */
  TC_CMS_OTHER_ATTRIBUTES_SKIP_LISTED = 0,
  /* Every attribute outside the selected identifiers returns UNSUPPORTED. */
  TC_CMS_OTHER_ATTRIBUTES_REJECT = 1,
  /* Skip listed attributes as above and any other attribute with a nonempty
   * value SET. DER attributes also need that SET in DER order. */
  TC_CMS_OTHER_ATTRIBUTES_SKIP_ALL = 2
} TC_CMS_other_attributes;

/* CMS encoding and attribute policy shared by readers and verifiers.
 * A zero-initialized policy selects a BER envelope, DER signed attributes,
 * NULL rsaEncryption parameters (RFC 3370 section 3.2), CMS attribute
 * identifiers and skipping of the listed attributes. Readers and verifiers
 * copy the policy at entry, so it may overlap any input. An unknown enum
 * value returns ARGUMENT. */
typedef struct {
  TC_CMS_envelope_encoding envelope;
  TC_CMS_attribute_encoding attributes;
  TC_CMS_rsa_parameters rsa_parameters;
  TC_CMS_attribute_oids attribute_oids;
  TC_CMS_other_attributes other_attributes;
} TC_CMS_verification_policy;

typedef struct {
  TC_bytes encoded;
  TC_bytes digest_algorithms, content_type, content;
  TC_bytes certificates, revocations, signers;
  uint32_t version;
  int has_content;
} TC_CMS_signed_data;

#if TC_ENABLE_CMS
/* Read a ContentInfo carrying SignedData and check the SignedData version
 * against its contents (RFC 5652 sections 3 and 5.1).
 * - encoded: the complete ContentInfo. policy->envelope selects BER or DER
 *   framing. Other policy fields are validated and otherwise unused here.
 * - limits bound framing. frames holds one frame per nesting level.
 * - out: content_type holds OID contents. Other spans keep complete
 *   encodings. content is an OCTET STRING, constructed only in BER.
 *   has_content distinguishes detached content from an empty embedded value.
 *   Absent optional collections have zero-length spans.
 * All spans borrow encoded, which stays alive and unchanged while they are in
 * use. encoded, policy, limits, frames, work and out must be disjoint.
 *
 * Work: a 10-unit storage check, then the encoded bytes of each parsing pass,
 * including the version pass over certificates, revocations and signers.
 * Returns OK with out written. ARGUMENT for NULL policy, limits, work or out,
 * an unknown policy value or overlapping storage, with all state unchanged.
 * LIMIT for exhausted limits, frames or work. INVALID for missing or extra
 * fields, wrong tags, framing outside the envelope encoding, a version that
 * differs from the RFC 5652 section 5.1 rule and trailing bytes. Failures
 * leave out unchanged. Frames and work are provisional after the argument
 * checks. Read signers with TC_CMS_signers_init and check the selected
 * signer's digest with TC_CMS_digest_algorithms_check. */
TC_TLV_result TC_CMS_signed_data_read(TC_bytes encoded, const TC_CMS_verification_policy* policy,
                                      const TC_TLV_limits* limits, TC_TLV_frames frames,
                                      size_t* work, TC_CMS_signed_data* out);
#endif

typedef struct {
  TC_bytes encoded;
  TC_bytes issuer, serial, subject_key_id;
  TC_DER_algorithm digest_algorithm, signature_algorithm;
  TC_bytes signed_attributes, unsigned_attributes, signature;
  uint32_t version;
  int serial_negative;
} TC_CMS_signer_info;

#if TC_ENABLE_CMS
/* Read one complete SignerInfo under policy->envelope (RFC 5652 section 5.3).
 * Version 1 identifies the signer by issuer and serial, version 3 by
 * subject_key_id. issuer is an encoded Name. serial keeps INTEGER contents,
 * including sign padding. subject_key_id is the complete implicit OCTET
 * STRING and signature a complete OCTET STRING. Either may be constructed in
 * a BER envelope. Attribute spans keep their encodings. Read signed
 * attributes with TC_CMS_signed_attributes_read.
 *
 * Checks identifier and version consistency, Name and algorithm syntax and
 * field framing. Algorithm selection, signature verification and signer
 * trust are separate steps. Spans borrow encoded. Storage, work, status and
 * failure rules match TC_CMS_signed_data_read. */
TC_TLV_result TC_CMS_signer_info_read(TC_bytes encoded, const TC_CMS_verification_policy* policy,
                                      const TC_TLV_limits* limits, TC_TLV_frames frames,
                                      size_t* work, TC_CMS_signer_info* out);

/* Start reading SignedData.signers, including its SET tag, under
 * policy->envelope. Checks the SET framing. TC_CMS_signer_next applies each
 * member's schema with the same envelope encoding. An empty SET is valid.
 * encoded, policy, limits, frames, work and out must be disjoint. Keep encoded
 * stable while the reader is in use and treat the reader as managed state.
 *
 * Work: a 10-unit storage check plus the encoded bytes. Returns OK with out
 * initialized. ARGUMENT for NULL arguments, an unknown policy value or
 * overlap, with all state unchanged. LIMIT for exhausted limits, frames or
 * work. INVALID for a wrong tag or bad framing. out changes only on OK. */
TC_TLV_result TC_CMS_signers_init(TC_bytes encoded, const TC_CMS_verification_policy* policy,
                                  const TC_TLV_limits* limits, TC_TLV_frames frames, size_t* work,
                                  TC_TLV_reader* out);
/* Read the next SignerInfo from a reader made by TC_CMS_signers_init.
 * reader, its input, frames, work and out must be disjoint. Output spans
 * borrow the reader input and stay valid after later reader calls. Share one
 * work budget across the init and next calls.
 *
 * Work: 10 units for the storage check, then the member's parsing passes.
 * Returns OK with the reader advanced and out written. END when no
 * member remains, charging no work and leaving all storage unchanged, even
 * with zero work remaining. ARGUMENT for NULL arguments, overlap or a reader
 * with a foreign profile or offset. LIMIT and INVALID follow
 * TC_CMS_signer_info_read. The reader and out change only on OK. Frames and
 * work are provisional on failure. */
TC_TLV_result TC_CMS_signer_next(TC_TLV_reader* reader, TC_TLV_frames frames, size_t* work,
                                 TC_CMS_signer_info* out);

/* Check that digest_algorithm, usually a SignerInfo.digest_algorithm, is
 * listed in signed_data->digest_algorithms with valid parameters. RFC 5652
 * section 5.1 lists every signer's digest there. Equal parameters may use
 * distinct BER encodings. Other listed algorithms need valid syntax only,
 * because they may belong to other signers.
 * - signed_data comes from TC_CMS_signed_data_read and keeps its input stable.
 * - signed_data, digest_algorithm, limits and their bytes may overlap each
 *   other. frames, work and out must be disjoint from them and each other.
 *
 * Work: one unit per storage comparison, then the selected OID and parameter
 * bytes, and for each listed algorithm its OID, the selected OID and the
 * parameters of a matching entry.
 * Returns OK and writes the SHA-1 or SHA-2 identifier to out. The identifier
 * names the hash for every build configuration, and the hashing call
 * reports a disabled hash as UNSUPPORTED. ARGUMENT for NULL arguments, a NULL
 * digestAlgorithms or OID span, or overlap, with all state unchanged. LIMIT
 * for exhausted limits, frames or work. UNSUPPORTED for a selected digest
 * outside SHA-1 and SHA-2. INVALID for bad syntax or parameters, or an
 * unlisted digest. out changes only on OK. */
TC_TLV_result TC_CMS_digest_algorithms_check(const TC_CMS_signed_data* signed_data,
                                             const TC_DER_algorithm* digest_algorithm,
                                             const TC_TLV_limits* limits, TC_TLV_frames frames,
                                             size_t* work, TC_hash_algorithm* out);
#endif

typedef struct {
  TC_bytes content_type, message_digest;
  TC_bytes signature_input[2];
  TC_X509_time signing_time;
  int has_signing_time;
  /* Encoded SMIMECapabilities sequence. Absent when data is NULL. */
  TC_bytes smime_capabilities;
  /* Encoded Name from the optional pivSigner-DN attribute. */
  TC_bytes signer_name;
  /* Original FASC-N OID and encoded OCTET STRING, including BER chunks. */
  TC_bytes fascn_oid, fascn_octets;
  /* Encoded entryUUID OCTET STRING (16 value bytes). */
  TC_bytes entry_uuid_octets;
} TC_CMS_signed_attributes;

#if TC_ENABLE_CMS
/* Read the complete IMPLICIT [0] signedAttrs field (RFC 5652 sections 5.3
 * and 11).
 * - policy->attributes selects DER, or BER_DEFINITE_ORDER for signatures made
 *   over unsorted attributes. Both require contentType and messageDigest
 *   exactly once and reject indefinite lengths. DER also requires SET OF order
 *   (X.690 section 11.6).
 * - policy->attribute_oids selects the interpreted identifiers: optional
 *   signingTime, SMIMECapabilities and entryUUID in every set, pivSigner-DN
 *   and pivFASC-N in the PIV sets, and twicFASC-N in PIV_TWIC. FASC-N values
 *   hold 25 bytes and entryUUID values 16 bytes. fascn_oid holds the
 *   identifier that matched. policy->other_attributes handles the rest.
 * - signature_input replaces the [0] tag with the SET OF tag and borrows the
 *   original length and contents (RFC 5652 section 5.4).
 * signingTime is asserted by the signer. Capabilities keep their advertised
 * order and opaque parameters. Path-building APIs match signer_name to the
 * certificate subject. Signature-only APIs leave that binding to the caller.
 * Spans borrow encoded. encoded, policy, limits, frames, work and out must be
 * disjoint.
 *
 * Work: a 10-unit storage check, the encoded bytes, each attribute's encoded
 * bytes and the bytes compared by the repeated-type scan.
 * Returns OK with out written. ARGUMENT for NULL arguments, an unknown policy
 * value or overlap, with all state unchanged. LIMIT for exhausted limits,
 * frames or work. UNSUPPORTED for an attribute that the other-attribute
 * policy rejects, and for twicFASC-N under the PIV set. INVALID for bad
 * framing or order, a missing or repeated required attribute, a repeated
 * attribute type or a value of the wrong size. out changes only on OK.
 * Frames and work are provisional on failure.
 *
 * Parsing only: the caller checks content type and digest, verifies the
 * signature over signature_input and validates the signer. */
TC_TLV_result TC_CMS_signed_attributes_read(TC_bytes encoded,
                                            const TC_CMS_verification_policy* policy,
                                            const TC_TLV_limits* limits, TC_TLV_frames frames,
                                            size_t* work, TC_CMS_signed_attributes* out);

/* Hash SignedData.content given as its complete OCTET STRING encoding. BER
 * chunk headers and end markers are excluded, so the digest covers the value
 * bytes (RFC 5652 section 5.4). For detached content, hash the application
 * message with the ordinary hash API.
 * - digest receives the full digest of algorithm. Size it with the hash's
 *   digest length. Bytes past that length are left alone.
 * - encoded, limits, frames, work and the whole digest buffer must be
 *   disjoint. One hash context lives on the stack and is wiped on return.
 *
 * Work: a 10-unit storage check, the encoded bytes and the hashed bytes.
 * Returns OK with the digest written. ARGUMENT for NULL arguments or overlap,
 * with all state unchanged. UNSUPPORTED for an unknown hash or one disabled in
 * this build. LIMIT for a short digest buffer, before any parsing, or for
 * exhausted limits, frames or work. INVALID for a value other than an OCTET
 * STRING or bad framing. digest changes only on OK. */
TC_TLV_result TC_CMS_content_digest(TC_bytes encoded, TC_hash_algorithm algorithm,
                                    const TC_TLV_limits* limits, TC_TLV_frames frames, size_t* work,
                                    TC_buffer digest);

/* Compare parsed signed attributes with the expected content type OID
 * contents and a digest computed over the content value bytes (RFC 5652
 * section 11.2). The digest may come from external hashing and can be reused
 * for signers that share the digest algorithm.
 * - digest.length must equal the digest length of algorithm.
 * - attributes and the spans may overlap each other. work and matched must be
 *   disjoint from them and from each other.
 *
 * Work: one unit per storage comparison, then the expected type bytes and the
 * digest length. The digest comparison is constant time.
 * Returns OK and writes matched as 1 for a match and 0 otherwise. ARGUMENT for
 * NULL arguments, empty type spans, a NULL digest, a digest of the wrong
 * length or overlap. UNSUPPORTED for an algorithm outside SHA-1 and SHA-2.
 * Both leave work and matched unchanged. LIMIT for exhausted work, with
 * matched unchanged. Signature and trust checks are separate steps. */
TC_TLV_result TC_CMS_content_digest_check(const TC_CMS_signed_attributes* attributes,
                                          TC_bytes expected_type, TC_hash_algorithm algorithm,
                                          TC_bytes digest, size_t* work, int* matched);
#endif

typedef struct {
  /* One frame per constructed nesting level of the deepest object parsed. */
  TC_TLV_frames frames;
  /* Needed only for a signature split across BER chunks. NULL/0 is allowed.
   * Capacity must hold the decoded signature. */
  uint8_t* signature;
  size_t signature_capacity;
} TC_CMS_signature_workspace;

/* One SignerInfo verification.
 * - signer and key come from their schema readers and retain stable input.
 * - content_type is the envelope's eContentType OID contents.
 * - policy selects the envelope and signed-attribute encodings, the
 *   interpreted attribute identifiers and the rsaEncryption parameter rule.
 *   The zero-initialized policy is the RFC 5652 / RFC 3370 section 3.2
 *   default. ALLOW_ABSENT applies only to rsaEncryption in SignerInfo. Present
 *   parameters must encode NULL.
 * - provider verifies the signature. limits bound every decode. */
typedef struct {
  const TC_CMS_signer_info* signer;
  TC_bytes content_type;
  TC_CMS_verification_policy policy;
  const TC_X509_public_key* key;
  const TC_X509_signature_provider* provider;
  const TC_TLV_limits* limits;
} TC_CMS_signer_verify_request;

#if TC_ENABLE_CMS
/* Verify one parsed SignerInfo against an independently computed digest of
 * the content under SignerInfo.digest_algorithm. For PSS, the
 * signed-attribute hash may differ. Checks algorithm and key compatibility,
 * the signed attributes, their content type and digest binding, and the
 * signature (RFC 5652 sections 5.4 and 5.6). Without signed attributes the
 * content type must be id-data (RFC 5652 section 5.3). Verification ignores
 * unsigned attributes, including countersignatures. RFC 5652 sections 5.3
 * and 11.4 exclude them from the signature, so they stay unauthenticated.
 * - The request, its bytes and the digest may overlap each other. Frames,
 *   workspace->signature and work must be disjoint from them and each other.
 *   The provider context and its scratch stay separate from all CMS storage.
 * - workspace->signature holds a signature split across BER chunks. Size it
 *   for the largest supported signature, or pass NULL/0 for DER signatures.
 * - The signed-attribute hash and a copy of its digest use stack scratch that
 *   is wiped on return.
 *
 * Work: one unit per storage comparison, algorithm resolution, the
 * signed-attribute pass, the hashed signature input and the provider charge
 * described in docs/api.md.
 * Returns VALID when the signature authenticates the digest with the supplied
 * key. ERROR for NULL arguments, an unknown policy value, overlap, an empty
 * content type, a NULL digest, or a digest whose length differs from the
 * resolved hash. The length check runs after algorithm resolution and so
 * after some work. UNSUPPORTED for a provider without verify_digest, an
 * unsupported algorithm or key, or a disabled hash. LIMIT for exhausted
 * limits, frames, work or signature capacity. INVALID for malformed fields,
 * a content type or digest mismatch, or a failed signature.
 *
 * The caller checks signer and certificate identity, trust, application
 * algorithm policy and the envelope's digestAlgorithms with
 * TC_CMS_digest_algorithms_check. */
TC_X509_signature_result TC_CMS_signer_verify_digest(const TC_CMS_signer_verify_request* request,
                                                     TC_bytes digest,
                                                     const TC_CMS_signature_workspace* workspace,
                                                     size_t* work);
#endif

typedef enum { TC_CMS_CONTENT_RAW, TC_CMS_CONTENT_BER_OCTETS } TC_CMS_content_encoding;

#if TC_ENABLE_CMS
/* Hash content and verify one parsed signer. The hash comes from
 * SignerInfo.digest_algorithm. RAW takes application content, including
 * NULL/0 for an empty message, bounded by limits->max_input and max_value.
 * BER_OCTETS takes SignedData.content with its complete OCTET STRING encoding
 * under the framing limits. The content hash reuses the stack scratch of the
 * signed-attribute hash and never copies the message.
 *
 * Storage, workspace, provider, trust and unsigned-attribute rules and
 * statuses match TC_CMS_signer_verify_digest. An unknown encoding returns
 * ERROR before any work. A disabled content hash returns UNSUPPORTED. Work
 * also covers the hashed content bytes. For cached or external digests, use
 * TC_CMS_signer_verify_digest. */
TC_X509_signature_result TC_CMS_signer_verify_content(const TC_CMS_signer_verify_request* request,
                                                      TC_bytes content,
                                                      TC_CMS_content_encoding encoding,
                                                      const TC_CMS_signature_workspace* workspace,
                                                      size_t* work);
#endif

#ifdef __cplusplus
}
#endif
#endif
