/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef TINY_CRYPTO_CMS_H_
#define TINY_CRYPTO_CMS_H_
#include <tiny_crypto/x509.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  TC_bytes encoded;
  TC_bytes digest_algorithms, content_type, content;
  TC_bytes certificates, revocations, signers;
  uint32_t version;
  int has_content;
} TC_CMS_signed_data;

/* Read a ContentInfo carrying SignedData, including its version consistency.
 * CMS envelopes use BER; requires TC_TLV_ENABLE_BER and TC_ENABLE_X509.
 * content_type holds OID contents. Other spans retain complete encodings;
 * content is an OCTET STRING, possibly constructed. has_content distinguishes
 * detached content from an embedded empty value. Missing optional collections
 * have zero-length spans. Collections require their own typed readers.
 * Use the certificate and revocation readers to parse collection members.
 * Signature and trust validation are separate steps. Select the signed-attribute
 * encoding when reading or verifying attributes.
 *
 * All spans borrow input, which must remain alive and unchanged. Input, limits,
 * frames, work and out must be disjoint. Bad storage arguments leave them
 * unchanged; parsing may consume work and frames. out changes only on OK.
 * Limits bound framing; work covers storage checks and all parsing passes. */
TC_TLV_result TC_CMS_signed_data_read(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_CMS_signed_data* out);

typedef struct {
  TC_bytes encoded;
  TC_bytes issuer, serial, subject_key_id;
  TC_DER_algorithm digest_algorithm, signature_algorithm;
  TC_bytes signed_attributes, unsigned_attributes, signature;
  uint32_t version;
  int serial_negative;
} TC_CMS_signer_info;

/* Read one complete SignerInfo using DER or BER explicitly. Version 1 uses
 * issuer/serial; version 3 uses subject_key_id. issuer is an encoded Name and
 * serial retains INTEGER contents, including sign padding. subject_key_id is
 * the complete implicit OCTET STRING; signature is a complete OCTET STRING.
 * Either OCTET STRING may be constructed in BER. Attribute spans retain their
 * encodings and need separate semantic/encoding checks.
 *
 * Checks identifier/version consistency, Name and algorithm syntax, and field
 * framing. Does not select algorithms, authenticate signatures or trust signers.
 * Spans borrow input. Storage, work and output rules match signed_data_read;
 * requires TC_ENABLE_X509, plus TC_TLV_ENABLE_BER for the BER profile. */
TC_TLV_result TC_CMS_signer_info_read(TC_bytes encoded, TC_TLV_profile profile,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t frame_capacity,
    size_t* work, TC_CMS_signer_info* out);

/* Initialize from SignedData.signers, including its SET tag. Checks BER tree
 * framing; next performs each member's schema checks. Empty sets are valid.
 * Input, limits, frames, work and out are disjoint; out changes only on OK.
 * Keep the encoded bytes stable and treat the returned reader as managed state. */
TC_TLV_result TC_CMS_signers_init(TC_bytes encoded, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_TLV_reader* out);
/* Reader and out change only on OK; frames/work are provisional on failure.
 * END leaves all storage unchanged, even with zero work remaining. Reuse one
 * budget across init/next calls. Reader, its input, frames, work and out must be
 * disjoint. Output spans borrow input, not scratch or reader state. */
TC_TLV_result TC_CMS_signer_next(TC_TLV_reader* reader, TC_TLV_frame* frames,
    size_t frame_capacity, size_t* work, TC_CMS_signer_info* out);

typedef enum {
  /* RFC 5652 signed attributes, including DER SET OF ordering. */
  TC_CMS_ATTRIBUTES_DER = 0,
  /* Application opt-in; requires TC_TLV_ENABLE_BER. Preserve definite length
   * octets and member order. No automatic detection or retry. */
  TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER = 1
} TC_CMS_attribute_encoding;

typedef enum {
  TC_CMS_RSA_PARAMETERS_NULL = 0,
  /* Accept an omitted rsaEncryption signatureAlgorithm parameter. */
  TC_CMS_RSA_PARAMETERS_ALLOW_ABSENT = 1
} TC_CMS_rsa_parameters;

typedef struct {
  TC_CMS_attribute_encoding attributes;
  TC_CMS_rsa_parameters rsa_parameters;
} TC_CMS_verification_policy;

typedef struct {
  TC_bytes content_type, message_digest;
  TC_bytes signature_input[2];
  TC_X509_time signing_time;
  int has_signing_time;
  /* Encoded SMIMECapabilities sequence; absent when data is NULL. */
  TC_bytes smime_capabilities;
  /* Encoded Name from the optional pivSigner-DN attribute. */
  TC_bytes signer_name;
  /* Original FASC-N OID and encoded OCTET STRING, including BER chunks. */
  TC_bytes fascn_oid, fascn_octets;
  /* Encoded entryUUID OCTET STRING (16 value bytes). */
  TC_bytes entry_uuid_octets;
} TC_CMS_signed_attributes;

/* Read the complete IMPLICIT [0] signedAttrs field. Select DER normally;
 * BER_DEFINITE_ORDER supports signatures made over unsorted attributes.
 * Both modes require content-type and message-digest exactly once, accept
 * optional signingTime, SMIMECapabilities, pivSigner-DN, PIV/TWIC FASC-N and entryUUID,
 * and reject indefinite lengths. FASC-N values contain exactly 25 bytes;
 * the application selects accepted namespaces using the returned fascn_oid.
 * Other attributes return UNSUPPORTED. signingTime is asserted by the signer.
 * Capabilities retain their advertised order and opaque parameter values;
 * applications interpret them after signature and trust validation.
 * Path-building APIs match signer_name to the certificate subject. Signature-only
 * APIs verify with the supplied key; the caller handles certificate/name binding.
 *
 * signature_input substitutes the SET OF tag and borrows the original length
 * and contents. Keep input alive and unchanged while using returned spans.
 * Input, limits, frames, work and out must be disjoint. Frames/work may change
 * on failure; out changes only on OK. Limits bound framing; work bounds bytes
 * examined across passes. Requires TC_ENABLE_X509.
 *
 * This parses attributes only. The caller must check content type and digest,
 * verify the signature over signature_input, and validate the signer's trust. */
TC_TLV_result TC_CMS_signed_attributes_read(TC_bytes encoded,
    TC_CMS_attribute_encoding encoding, const TC_TLV_limits* limits,
    TC_TLV_frame* frames, size_t frame_capacity, size_t* work, TC_CMS_signed_attributes* out);

/* Hash SignedData.content, including its complete OCTET STRING encoding.
 * BER chunk headers and end markers are excluded from the digest. For detached
 * content, hash the application's raw message with the ordinary hash API.
 * Requires TC_ENABLE_X509, TC_TLV_ENABLE_BER and the selected hash implementation.
 * Unknown or disabled hashes return UNSUPPORTED. A short digest buffer returns
 * LIMIT; success writes the algorithm's full digest and leaves spare bytes alone.
 *
 * Input, limits, frames, work and the entire digest buffer must be disjoint.
 * Bad storage leaves them unchanged. Other errors may consume frames/work but
 * preserve digest. Work covers storage checks, encoded bytes and hashed bytes.
 * Uses one temporary hash context on the stack; content is never flattened. */
TC_TLV_result TC_CMS_content_digest(TC_bytes encoded, TC_hash_algorithm algorithm,
    const TC_TLV_limits* limits, TC_TLV_frame* frames, size_t frame_capacity,
    size_t* work, uint8_t* digest, size_t digest_capacity);

/* Compare parsed signed attributes with the content type OID contents and a
 * computed digest. Hash content value bytes, excluding OCTET STRING framing.
 * Reuse that digest for signers using the same digest algorithm. The selected
 * algorithm determines the required digest length; hashing may be external.
 *
 * OK writes matched as 0 or 1; a match does not verify a signature or trust.
 * Errors leave matched unchanged. Unknown algorithms return UNSUPPORTED.
 * All inputs are borrowed and may overlap each other, but must be disjoint
 * from work and matched, which must also be disjoint. Invalid storage leaves
 * both unchanged; otherwise work covers storage checks and compared bytes.
 * Requires TC_ENABLE_X509. */
TC_TLV_result TC_CMS_content_digest_check(const TC_CMS_signed_attributes* attributes,
    TC_bytes expected_type, TC_hash_algorithm algorithm, TC_bytes digest,
    size_t* work, int* matched);

typedef struct {
  TC_TLV_frame* frames;
  size_t frame_capacity;
  /* Needed only for a signature split across BER chunks; NULL/0 is allowed.
   * Capacity is the decoded signature length, not the whole CMS object. */
  uint8_t* signature;
  size_t signature_capacity;
} TC_CMS_signature_workspace;

/* Verify one parsed SignerInfo against an independently computed content digest.
 * Hash content with SignerInfo.digest_algorithm. For PSS, the signed-attribute
 * hash may differ. content_type is the envelope's OID contents.
 * Checks digest binding, signed attributes, algorithm/key compatibility and the
 * signature. Without signed attributes, content_type must be id-data.
 * Select signed-attribute encoding explicitly; CMS field framing uses BER.
 * Countersignatures are not accepted by this operation.
 *
 * signer and key must come from their schema readers and retain stable input.
 * The caller checks signer/certificate identity, trust, application algorithm
 * policy and the envelope's digestAlgorithms.
 * A VALID result authenticates the supplied digest with the supplied key only.
 *
 * Inputs and all metadata may overlap each other, but not frames, signature
 * storage or work. Those writable ranges must also be disjoint. Provider context
 * and its scratch must be separate from all CMS inputs and workspace storage.
 * Bad storage leaves caller state unchanged; other failures may consume scratch
 * and work. Uses a temporary hash context/digest for signed attributes. Requires
 * X509, BER, a digest provider, and the signed-attribute hash when attributes exist. */
TC_X509_signature_result TC_CMS_signer_verify_digest(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes digest, TC_CMS_attribute_encoding encoding,
    const TC_X509_public_key* key, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_CMS_signature_workspace* workspace, size_t* work);

typedef enum {
  TC_CMS_CONTENT_RAW,
  TC_CMS_CONTENT_BER_OCTETS
} TC_CMS_content_encoding;

/* Hash content and verify one parsed signer, resolving its hash internally.
 * RAW accepts application content, including NULL/0 for an empty message.
 * BER_OCTETS accepts SignedData.content with its complete OCTET STRING encoding.
 * No format detection or fallback is performed. Raw content is bounded by
 * max_input and max_value; BER content also uses the framing limits.
 *
 * Storage, provider and trust rules match signer_verify_digest. The content hash
 * must be enabled. Hash scratch is reused for signed attributes, without copying
 * the message. For cached or externally computed digests, use the digest API. */
TC_X509_signature_result TC_CMS_signer_verify_content(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes content, TC_CMS_content_encoding content_encoding,
    TC_CMS_attribute_encoding attribute_encoding, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_CMS_signature_workspace* workspace, size_t* work);

/* Explicit compatibility policy for captured CMS signatures. ALLOW_ABSENT applies
 * only to rsaEncryption in SignerInfo; present parameters must encode NULL.
 * Certificate algorithms and RSA DigestInfo retain their own validation rules.
 * The ordinary verify functions use RSA_PARAMETERS_NULL (RFC 3370 section 3.2).
 * Storage, hashing and trust requirements match the corresponding functions above. */
TC_X509_signature_result TC_CMS_signer_verify_digest_with_policy(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes digest, TC_CMS_verification_policy policy,
    const TC_X509_public_key* key, const TC_X509_signature_provider* provider,
    const TC_TLV_limits* limits, const TC_CMS_signature_workspace* workspace, size_t* work);
TC_X509_signature_result TC_CMS_signer_verify_content_with_policy(const TC_CMS_signer_info* signer,
    TC_bytes content_type, TC_bytes content, TC_CMS_content_encoding content_encoding,
    TC_CMS_verification_policy policy, const TC_X509_public_key* key,
    const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
    const TC_CMS_signature_workspace* workspace, size_t* work);

#ifdef __cplusplus
}
#endif
#endif
