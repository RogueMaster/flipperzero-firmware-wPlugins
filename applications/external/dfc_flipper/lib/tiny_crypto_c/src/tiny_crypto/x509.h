/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* X.509 certificate and public-key readers, extension decoders, name
 * handling and signature verification through a provider.
 * Standards: RFC 5280, RFC 5480, RFC 4055, RFC 8017.
 * Configuration: TC_ENABLE_X509, which requires TC_ENABLE_DER.
 * Limitations: readers parse DER. Trust, validity and revocation are
 * separate steps.
 * Contracts: docs/api.md, including its size_t work units.
 * Guide: docs/x509-path.md. */
#ifndef TINY_CRYPTO_X509_H_
#define TINY_CRYPTO_X509_H_
#include <tiny_crypto/key.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Readers return TC_TLV_result with the meanings of docs/api.md: INVALID for
 * malformed or truncated received data, LIMIT for an exhausted TC_TLV_limits
 * bound, work budget or caller capacity, and for a received count or
 * distance above UINT32_MAX, UNSUPPORTED for a well-formed feature outside
 * this library, and ARGUMENT for NULL pointers, a span with NULL data and a
 * length, or a checked overlap. Returned spans borrow the input, which stays
 * alive and unchanged while they are used. Output objects stay disjoint from
 * the input. Functions without a work parameter charge no work. Functions
 * with one charge the size_t units of docs/api.md, and a LIMIT from an
 * exhausted budget sets *work to 0. */

typedef struct {
  unsigned year;
  uint8_t month, day, hour, minute, second;
} TC_X509_time;

typedef struct {
  TC_DER_algorithm algorithm;
  TC_bytes key, modulus, exponent, curve_oid;
  TC_key_type type;
  TC_EC_curve curve;
  unsigned bits;
} TC_X509_public_key;

#if TC_ENABLE_X509
/* Decode SubjectPublicKeyInfo (RFC 5280 section 4.1.2.7) and classify the
 * key. RSA keys (RFC 3279 section 2.3.1, RFC 4055 section 1.2) need an odd
 * modulus and an odd exponent with 3 <= e < n. EC keys (RFC 5480 section 2)
 * need namedCurve parameters and a compressed or uncompressed SEC 1 point.
 * curve names the NIST P, secp256k1 and brainpool curves the library
 * identifies, and the point length must match that curve. Another named curve
 * keeps curve TC_EC_UNKNOWN with bits 0. DSA keys read Dss-Parms when present
 * (RFC 3279 section 2.3.2). X25519, X448, Ed25519 and Ed448 keys (RFC 8410
 * section 3) have absent parameters and a fixed length. Other algorithms keep
 * their OID and key bytes with type TC_KEY_UNKNOWN. bits holds the modulus, p
 * or curve size. The caller checks EC points for curve membership with
 * TC_EC_validate_public_key.
 *
 * TC_TLV_ARGUMENT     NULL out, or encoded with NULL data and a length.
 * TC_TLV_INVALID      malformed or truncated DER, or a key that breaks the
 *                     checks above.
 * TC_TLV_UNSUPPORTED  RSASSA-PSS parameters with a mask other than MGF1.
 * TC_TLV_LIMIT        a key too long for unsigned bits.
 *
 * out changes only on OK. */
TC_TLV_result TC_X509_subject_public_key(TC_bytes encoded, TC_X509_public_key* out);
#endif

typedef struct {
  /* One frame per constructed nesting level of the deepest object parsed. */
  TC_TLV_frames frames;
  /* One slot per extension, used to detect duplicate OIDs. */
  TC_bytes* extension_oids;
  size_t extension_capacity;
} TC_X509_workspace;

typedef struct {
  TC_bytes encoded, tbs, serial, issuer, subject, spki, extensions, signature;
  TC_DER_algorithm signature_algorithm;
  TC_X509_public_key public_key;
  TC_X509_time not_before, not_after;
  unsigned version;
  int serial_negative;
} TC_X509_certificate;

#if TC_ENABLE_X509
/* Parse one DER certificate (RFC 5280 section 4.1). Returned spans borrow
 * encoded. The reader checks the TBSCertificate schema, versions 1 to 3,
 * unique identifiers only in v2 and v3, extensions only in v3, matching
 * outer and inner signature algorithms (section 4.1.1.2), a signature
 * encoding that fits its algorithm, unique extension OIDs (section 4.2), a
 * critical subjectAltName for an empty subject (section 4.1.2.6), and the
 * syntax of the basicConstraints, keyUsage, subjectAltName and
 * issuerAltName values. Interpret other extension values according to
 * their OIDs. Validate signatures, paths, revocation and time before
 * accepting the certificate.
 *
 * limits->max_elements counts every element parsed, including the DER
 * inside each extension value. workspace->frames needs one frame per
 * constructed nesting level and workspace->extension_oids one slot per
 * extension. out, the frames and the OID slots are written. They must be
 * pairwise disjoint and lie outside encoded, limits and the workspace
 * struct.
 *
 * TC_TLV_ARGUMENT     NULL limits, workspace or out, an array that is NULL
 *                     with a capacity, or overlap.
 * TC_TLV_INVALID      malformed or truncated DER or a broken schema rule.
 * TC_TLV_LIMIT        a limits bound, the frames or the extension slots ran
 *                     out, or a basicConstraints path length above
 *                     UINT32_MAX.
 * TC_TLV_UNSUPPORTED  RSASSA-PSS parameters or a key with a mask other than
 *                     MGF1.
 *
 * Frames and OID slots may change on failure. out changes only on OK. */
TC_TLV_result TC_X509_read(TC_bytes encoded, const TC_TLV_limits* limits,
                           const TC_X509_workspace* workspace, TC_X509_certificate* out);
#endif
/* Time helpers. A valid TC_X509_time is a Gregorian UTC calendar time in
 * years 1..9999 with hour <= 23, minute <= 59 and second <= 59, without leap
 * seconds (RFC 5280 section 4.1.2.5). Each returns ARGUMENT for a NULL
 * pointer or an output overlapping an input, INVALID for an invalid time and
 * OK otherwise. Outputs change only on OK. */

#if TC_ENABLE_X509
/* *order receives -1, 0 or 1 as left is before, equal to or after right. */
TC_TLV_result TC_X509_time_compare(const TC_X509_time* left, const TC_X509_time* right, int* order);
/* Check value alone. ARGUMENT only for NULL. */
TC_TLV_result TC_X509_time_check(const TC_X509_time* value);
/* Convert to seconds since 1970-01-01T00:00:00Z. Earlier dates give negative
 * values. The result is independent of platform time_t and timezone state. */
TC_TLV_result TC_X509_time_to_unix(const TC_X509_time* value, int64_t* seconds);
/* *valid receives 1 when notBefore <= at <= notAfter (RFC 5280 section
 * 4.1.2.5), otherwise 0. valid must be disjoint from certificate, its
 * encoding and at. A reversed validity interval returns INVALID. This checks
 * the validity interval alone. */
TC_TLV_result TC_X509_valid_at(const TC_X509_certificate* certificate, const TC_X509_time* at,
                               int* valid);
#endif

typedef TC_result TC_X509_signature_result;
#define TC_X509_SIGNATURE_VALID TC_RESULT_OK
#define TC_X509_SIGNATURE_INVALID TC_RESULT_INVALID
#define TC_X509_SIGNATURE_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_X509_SIGNATURE_ERROR TC_RESULT_ERROR
#define TC_X509_SIGNATURE_LIMIT TC_RESULT_LIMIT
typedef TC_X509_signature_result (*TC_X509_signature_verify_fn)(
    void* context, const TC_bytes* message, size_t count, const TC_DER_algorithm* algorithm,
    TC_bytes signature, const TC_X509_public_key* issuer_key, size_t* work);
typedef TC_X509_signature_result (*TC_X509_signature_verify_digest_fn)(
    void* context, TC_bytes digest, const TC_signature_algorithm* algorithm, TC_bytes signature,
    const TC_X509_public_key* issuer_key, size_t* work);
typedef struct {
  TC_X509_signature_verify_fn verify;
  void* context;
  /* Optional prehashed operation for CMS and other externally hashed content. */
  TC_X509_signature_verify_digest_fn verify_digest;
} TC_X509_signature_provider;
/* Supplied through the application's trusted configuration. A self-signed
 * certificate can supply these fields. Trust comes from the configuration,
 * independent of the self-signature. Anchor constraints and policy settings
 * belong to path inputs. */
typedef struct {
  TC_bytes name;
  TC_X509_public_key public_key;
} TC_X509_trust_anchor;
#if TC_ENABLE_X509
/* Signature verification through a provider. Results:
 *
 *   VALID        the provider verified the signature under the key.
 *   INVALID      the provider rejected the signature, or for
 *                TC_X509_issuer_check the names differ.
 *   UNSUPPORTED  a NULL provider or missing callback, or the provider
 *                lacks the algorithm.
 *   LIMIT        the work budget ran out.
 *   ERROR        a NULL key, algorithm or work, an empty signature, key or
 *                algorithm OID, a digest of the wrong length, an input
 *                overlapping work, a provider failure, or a callback that
 *                returned more work than it received.
 *
 * Work: before the callback, each verification charges one unit plus the
 * byte length for each signed-data span (message segment or digest), the
 * signature, the DER algorithm OID and parameters (message forms), and the
 * issuer key's algorithm OID, parameters and key. The key's modulus,
 * exponent and curve_oid are parts of key and share its charge. A budget
 * exhausted before the callback returns LIMIT with *work set to 0. The
 * callback then consumes its own work. A callback that returns more work
 * than it received makes the result ERROR with *work set to 0. Inputs,
 * provider metadata and the key stay unchanged and disjoint from work.
 * Provider context and work may change on failure.
 *
 * Verify a parsed certificate's signature (RFC 5280 section 4.1.1.3). The
 * callback receives one message segment holding the original DER
 * TBSCertificate with its header, the signature bytes without the BIT STRING
 * header and unused-bits count, and unmodified algorithm parameters. It must
 * check algorithm and key compatibility and return VALID only after
 * cryptographic verification. Issuer identity, validity and trust are
 * separate checks. */
TC_X509_signature_result TC_X509_signature_verify(const TC_X509_certificate* certificate,
                                                  const TC_X509_public_key* issuer_key,
                                                  const TC_X509_signature_provider* provider,
                                                  size_t* work);
/* Verify the ordered concatenation of count borrowed message segments. Empty
 * segments are allowed, and count zero is an empty message. The provider
 * hashes each segment in order and checks algorithm and key compatibility.
 * Results and work follow the rules above. For a supplied digest, use
 * TC_X509_signature_verify_digest. */
TC_X509_signature_result TC_X509_signature_verify_message(
    const TC_bytes* message, size_t count, const TC_DER_algorithm* algorithm, TC_bytes signature,
    const TC_X509_public_key* issuer_key, const TC_X509_signature_provider* provider, size_t* work);
#endif

#if TC_ENABLE_X509
/* Verify a digest as supplied through provider->verify_digest. algorithm
 * describes the signature, including its digest hash and PSS MGF hash and
 * salt length. digest.length must equal the hash's digest length, otherwise
 * the result is ERROR. An unknown hash returns UNSUPPORTED. Providers check
 * key restrictions and verify the signature mathematically. Results and work
 * follow the rules above. VALID authenticates the digest under the key.
 * Digest origin and certificate trust are separate checks. */
TC_X509_signature_result
TC_X509_signature_verify_digest(TC_bytes digest, const TC_signature_algorithm* algorithm,
                                TC_bytes signature, const TC_X509_public_key* issuer_key,
                                const TC_X509_signature_provider* provider, size_t* work);
#endif

typedef struct {
  TC_bytes encoded, oid, value;
} TC_X509_name_attribute;
#if TC_ENABLE_X509
/* Iterate a DER Name (RFC 5280 section 4.1.2.4). init takes the complete
 * Name SEQUENCE and starts reader, which must lie outside encoded.
 * rdn_next returns the next RDN, a nonempty SET of DER-sorted attributes, as
 * its SET contents, suitable for a TLV reader passed to attribute_next.
 * attribute_next returns one AttributeTypeAndValue whose value keeps its
 * complete encoding. Known X.520 attributes, userId, emailAddress and
 * domainComponent must use their permitted string types, and string values
 * must hold valid characters for their type. Other value types remain
 * opaque for the caller to check. Element limits count the Name, SETs,
 * attribute sequences, OIDs and values, and depth counts each opened level.
 *
 * Results: OK, END after the last item, INVALID for malformed data, a wrong
 * string type, an empty RDN or unsorted attributes, LIMIT for an exhausted
 * bound, and ARGUMENT for a NULL pointer, a reader of another profile or an
 * init reader inside encoded. The reader and outputs change only on OK. */
TC_TLV_result TC_X509_name_init(TC_TLV_reader* reader, TC_bytes encoded,
                                const TC_TLV_limits* limits);
TC_TLV_result TC_X509_rdn_next(TC_TLV_reader* reader, TC_bytes* out);
TC_TLV_result TC_X509_attribute_next(TC_TLV_reader* reader, TC_X509_name_attribute* out);
#endif

typedef struct {
  uint32_t *left, *right;
  size_t scalar_capacity;
  uint8_t* matched;
  size_t attribute_capacity;
} TC_X509_name_workspace;
#if TC_ENABLE_X509
/* Compare DER Names using RFC 5280 section 7.1. name_equal requires the same
 * RDN sequence. name_within reports whether subtree is a leading prefix of
 * name, the directoryName constraint rule of RFC 5280 section 4.2.1.10.
 * Standard X.520 case-ignore attributes and userId use RFC 4518 preparation.
 * domainComponent and emailAddress (RFC 2985 section 5.2.1) use ASCII
 * case-insensitive comparison. Values without a supported preparation, such
 * as TeletexString or an attribute type with no known matching rule, match
 * only when their encodings are identical. When such a difference is all that
 * separates the names, the result is UNSUPPORTED and matched is unchanged. A
 * difference in any other attribute or in the RDN structure still returns OK
 * with matched zero. Domain label validity beyond ASCII is a separate check.
 * Workspace sizing: scalar_capacity uint32_t code points in each of left and
 * right for the longest prepared attribute value, and attribute_capacity
 * bytes for the most attributes in one RDN. All writable ranges must be
 * disjoint from each other and from the inputs.
 *
 * TC_TLV_ARGUMENT     NULL limits, workspace, work or matched, workspace
 *                     arrays NULL with a capacity, or overlap.
 * TC_TLV_INVALID      a malformed Name.
 * TC_TLV_LIMIT        a limits bound, the workspace or work ran out.
 * TC_TLV_UNSUPPORTED  the names differ only in values without a supported
 *                     preparation.
 *
 * Work: traversed bytes, prepared scalars and compared values. Workspace and
 * work may change on failure. matched changes only on OK. */
TC_TLV_result TC_X509_name_equal(TC_bytes left, TC_bytes right, const TC_TLV_limits* limits,
                                 const TC_X509_name_workspace* workspace, size_t* work,
                                 int* matched);
TC_TLV_result TC_X509_name_within(TC_bytes name, TC_bytes subtree, const TC_TLV_limits* limits,
                                  const TC_X509_name_workspace* workspace, size_t* work,
                                  int* matched);
#endif
#if TC_ENABLE_X509
/* Check one issuer link: certificate->issuer equals issuer_name under
 * TC_X509_name_equal, then TC_X509_signature_verify with issuer_key (RFC
 * 5280 section 6.1.3 steps (a)(1) and (a)(4)). issuer_name and issuer_key
 * may come from a candidate issuer or a configured trust anchor. workspace
 * is the name workspace, required. Inputs and provider context stay disjoint
 * from scratch and work. Results follow the signature rules above. Names
 * that differ return INVALID, names that differ only in unprepared values
 * return UNSUPPORTED, and a malformed name returns INVALID. Work: the name
 * comparison, then the signature charges. VALID covers this link only.
 * Issuer trust, CA authorization, validity, path and revocation checks are
 * separate. */
TC_X509_signature_result
TC_X509_issuer_check(const TC_X509_certificate* certificate, TC_bytes issuer_name,
                     const TC_X509_public_key* issuer_key,
                     const TC_X509_signature_provider* provider, const TC_TLV_limits* limits,
                     const TC_X509_name_workspace* workspace, size_t* work);
#endif

typedef struct {
  TC_bytes oid, value;
  int critical;
} TC_X509_extension;
typedef struct {
  TC_bytes key_identifier, issuer, serial;
  int has_key_identifier, serial_negative;
} TC_X509_authority_key_identifier;
/* Extension-value decoders (RFC 5280 section 4.2).
 * Each decoder and reader init below takes one complete extension value, the
 * bytes inside the extnValue OCTET STRING, unless it names a contents span.
 * Returned spans borrow those bytes, which must stay unchanged while the
 * spans are used. max_elements counts the outer element and every element the
 * decoder reads beneath it. max_depth counts the constructed levels it opens,
 * the outer SEQUENCE included. A constructed field returned as a span, such as
 * an AKI issuer or a name-constraint list, counts as one element. Decode it
 * under its own limits. A reader's next spends the budget left by earlier
 * calls, so the limits bound the whole list. Exhausted limits return LIMIT.
 * Malformed or truncated values return INVALID. NULL arguments return
 * ARGUMENT, as does a reader inside its input at init. next returns END after
 * the last item. Outputs, and reader state for next, change only on OK. */

#if TC_ENABLE_X509
/* AuthorityKeyIdentifier (RFC 5280 section 4.2.1.1) and
 * SubjectKeyIdentifier (section 4.2.1.2). Identifiers have no assumed hash
 * or fixed length. Authority issuer and serial must occur together, and an
 * empty issuer is INVALID. issuer contains GeneralNames contents for
 * TC_X509_general_names_contents_init. serial keeps its signed INTEGER
 * contents. An empty AuthorityKeyIdentifier is syntactically valid. The
 * SubjectKeyIdentifier output is the OCTET STRING contents.
 * Certificate-profile requirements and issuer selection are separate checks. */
TC_TLV_result TC_X509_authority_key_identifier_read(TC_bytes value, const TC_TLV_limits* limits,
                                                    TC_X509_authority_key_identifier* out);
TC_TLV_result TC_X509_subject_key_identifier_read(TC_bytes value, const TC_TLV_limits* limits,
                                                  TC_bytes* out);
#endif
typedef struct {
  unsigned type;
  TC_bytes encoded, value;
} TC_X509_general_name;
/* GeneralNames reader (RFC 5280 section 4.2.1.6) for subject and issuer
 * alternative names, AKI issuers and CRL names. Name-constraint subtrees use
 * the subtree reader below. type is the ASN.1 choice number (0..8). IP
 * addresses are 4 or 16 bytes. Spans borrow input. x400Address and
 * ediPartyName retain their DER structure. Their schemas and otherName's
 * OID-specific value require caller interpretation. init binds the input,
 * limits and frames. frames need one entry per constructed level inside one
 * GeneralName. next walks each name's complete tree and charges every nested
 * element to the limits. Consume through END to check the full list. Frames
 * may change on failure. Reader and out change only on OK. Input, frames,
 * reader and out must be disjoint. init returns ARGUMENT for a NULL reader,
 * NULL frames with a capacity, or overlap between reader, input and frames.
 * next returns INVALID for an unknown choice, an empty or non-ASCII
 * rfc822Name, dNSName or URI, a control or space in a dNSName or URI, an IP
 * address of another length, or a malformed directoryName, registeredID or
 * otherName, and ARGUMENT for a NULL reader or out. Keep the input and frames
 * alive and unchanged while the reader is used. */
typedef struct {
  TC_TLV_reader reader;
  TC_TLV_frames frames;
} TC_X509_general_names_reader;
#if TC_ENABLE_X509
/* encoded is a complete, nonempty GeneralNames SEQUENCE, such as a
 * subjectAltName or issuerAltName extension value. */
TC_TLV_result TC_X509_general_names_init(TC_X509_general_names_reader* reader, TC_bytes encoded,
                                         const TC_TLV_limits* limits, TC_TLV_frames frames);
/* contents holds GeneralNames fields without their SEQUENCE header, such as
 * authority_key_identifier.issuer or a CRL fullName. limits cover contents
 * alone. Empty contents return END from the first next. */
TC_TLV_result TC_X509_general_names_contents_init(TC_X509_general_names_reader* reader,
                                                  TC_bytes contents, const TC_TLV_limits* limits,
                                                  TC_TLV_frames frames);
TC_TLV_result TC_X509_general_name_next(TC_X509_general_names_reader* reader,
                                        TC_X509_general_name* out);
#endif
typedef struct {
  TC_bytes permitted, excluded;
} TC_X509_name_constraints;
#if TC_ENABLE_X509
/* NameConstraints (RFC 5280 section 4.2.1.10): the two optional, nonempty
 * subtree lists, at least one present. Spans contain the IMPLICIT sequence
 * contents, without a SEQUENCE wrapper. Decode them with
 * TC_X509_general_subtrees_init. out is unchanged on failure. */
TC_TLV_result TC_X509_name_constraints_read(TC_bytes value, const TC_TLV_limits* limits,
                                            TC_X509_name_constraints* out);
#endif
typedef struct {
  TC_X509_general_name base;
  uint32_t minimum, maximum;
  int has_maximum;
} TC_X509_general_subtree;
/* GeneralSubtrees reader over a permitted or excluded contents span from
 * TC_X509_name_constraints_read. Consume through END. IP bases contain address
 * and mask (8 or 32 bytes). Distances above UINT32_MAX return LIMIT, and an
 * explicit minimum of zero returns INVALID because DER omits the DEFAULT. RFC
 * 5280 path validation requires minimum zero and maximum absent. This decoder
 * retains other encoded values. limits cover contents alone. frames need one
 * entry per constructed level inside one GeneralSubtree. Binding, budget,
 * borrowing and failure rules match the GeneralNames reader. */
typedef struct {
  TC_TLV_reader reader;
  TC_TLV_frames frames;
} TC_X509_general_subtrees_reader;
#if TC_ENABLE_X509
TC_TLV_result TC_X509_general_subtrees_init(TC_X509_general_subtrees_reader* reader,
                                            TC_bytes contents, const TC_TLV_limits* limits,
                                            TC_TLV_frames frames);
TC_TLV_result TC_X509_general_subtree_next(TC_X509_general_subtrees_reader* reader,
                                           TC_X509_general_subtree* out);
/* Test a decoded name against one subtree. Different name forms do not match,
 * except SmtpUTF8Mailbox otherName follows rfc822Name constraints (RFC 9598).
 * Supports directoryName, dNSName, rfc822Name, URI and CIDR iPAddress ranges.
 * DNS constraints include the base domain. A leading dot restricts them to
 * proper subdomains. Email constraints match an exact host, or proper
 * subdomains with a leading dot (RFC 9549). Mailbox-specific constraints and
 * address literals return UNSUPPORTED, as do DNS wildcards, other forms and
 * non-default distances. URI constraints use the email host/domain rules.
 * Missing authority hosts and IP-literal hosts return INVALID.
 * Percent-encoded unreserved host characters are decoded for matching, and
 * the decoded host must be a valid DNS host. SmtpUTF8Mailbox requires a
 * non-ASCII local part and lowercase domain labels. IDNA A-label validation
 * is separate. Workspace is required for directoryName only. Input
 * objects/spans, workspace, work and matched must be disjoint.
 *
 * TC_TLV_OK           *matched is 1 inside the subtree, otherwise 0.
 * TC_TLV_ARGUMENT     a NULL pointer, a type above 8, a NULL workspace for
 *                     directoryName, or overlap.
 * TC_TLV_INVALID      a malformed name or base, such as a non-contiguous IP
 *                     mask or a URI without a host.
 * TC_TLV_LIMIT        a value longer than limits allow, or work ran out.
 * TC_TLV_UNSUPPORTED  a form or constraint listed above as unsupported.
 *
 * Work: one unit, then the bytes the matcher examines. Workspace and work
 * may change on failure. matched changes only on OK. */
TC_TLV_result TC_X509_general_name_within(const TC_X509_general_name* name,
                                          const TC_X509_general_subtree* subtree,
                                          const TC_TLV_limits* limits,
                                          const TC_X509_name_workspace* workspace, size_t* work,
                                          int* matched);
#endif
typedef struct {
  TC_TLV_frames frames;
  const TC_X509_name_workspace* names;
} TC_X509_constraint_workspace;
#if TC_ENABLE_X509
/* Check one decoded name against one certificate's constraint lists. A name
 * must match at least one permitted subtree of its form, if any, and no
 * excluded subtree. Both lists are fully parsed. Empty spans mean no
 * constraints. Apply each issuer's lists separately to intersect their
 * permitted sets. names is needed only for directoryName. Input, scratch,
 * work and permitted must be disjoint. *permitted receives 1 or 0 (RFC 5280
 * section 6.1.3 steps (b) and (c)). Statuses follow
 * TC_X509_general_name_within, plus LIMIT when the lists exceed limits or the
 * frames run out. Work: the length of both lists, one unit per subtree and
 * the matcher's work. Scratch and work may change on failure. permitted
 * changes only on OK. */
TC_TLV_result TC_X509_name_constraints_check(const TC_X509_general_name* name,
                                             const TC_X509_name_constraints* constraints,
                                             const TC_TLV_limits* limits,
                                             const TC_X509_constraint_workspace* workspace,
                                             size_t* work, int* permitted);
/* Apply one issuer's constraints to a certificate returned by TC_X509_read
 * (RFC 5280 section 4.2.1.10). Checks the nonempty subject DN and every
 * subjectAltName. Without that extension, subject emailAddress attributes
 * also receive email constraints. An empty subject without subjectAltName
 * returns INVALID. The path validator decides when the self-issued exception
 * applies. Limits bound each parsed structure. Work bounds their combined
 * processing. Statuses, workspace and output rules match
 * name_constraints_check. */
TC_TLV_result TC_X509_certificate_names_check(const TC_X509_certificate* certificate,
                                              const TC_X509_name_constraints* constraints,
                                              const TC_TLV_limits* limits,
                                              const TC_X509_constraint_workspace* workspace,
                                              size_t* work, int* permitted);
/* Pass certificate.extensions, or {NULL, 0} for an absent extension sequence,
 * which returns END from the first next. A present sequence must be
 * nonempty. limits cover the Extensions SEQUENCE under the extension-value
 * rules above. next checks one Extension's fields (RFC 5280 section 4.1):
 * an explicit critical value must be TRUE because DER omits the DEFAULT, and
 * out->value is the extnValue contents. Consume through END to check every
 * extension. */
TC_TLV_result TC_X509_extensions_init(TC_TLV_reader* reader, TC_bytes encoded,
                                      const TC_TLV_limits* limits);
TC_TLV_result TC_X509_extension_next(TC_TLV_reader* reader, TC_X509_extension* out);
#endif

typedef struct {
  TC_bytes issuer_policy, subject_policy;
} TC_X509_policy_mapping;
#if TC_ENABLE_X509
/* Pass the PolicyMappings extension value (RFC 5280 section 4.2.1.5). init
 * checks the outer, nonempty sequence. next validates each OID pair and
 * returns INVALID for a mapping to or from anyPolicy. Consume through END to
 * check every pair. */
TC_TLV_result TC_X509_policy_mappings_init(TC_TLV_reader* reader, TC_bytes value,
                                           const TC_TLV_limits* limits);
TC_TLV_result TC_X509_policy_mapping_next(TC_TLV_reader* reader, TC_X509_policy_mapping* out);
#endif

typedef struct {
  TC_bytes oid, qualifiers;
} TC_X509_policy;
typedef struct {
  TC_TLV_reader reader;
  TC_bytes* seen;
  size_t capacity, count;
} TC_X509_policy_reader;
#if TC_ENABLE_X509
/* Pass the CertificatePolicies extension value (RFC 5280 section 4.2.1.4).
 * seen has one slot per policy, used to reject duplicate identifiers with
 * INVALID. More policies than capacity return LIMIT. Input, seen, reader and
 * output must be disjoint, and init and next return ARGUMENT for overlap.
 * next checks PolicyInformation structure. qualifiers retain their complete
 * SEQUENCE encoding for separate interpretation. The qualifier SEQUENCE
 * counts as one element here. An absent qualifier sequence is {NULL, 0}.
 * Consume through END. A failed next leaves reader, seen and out unchanged. */
TC_TLV_result TC_X509_policies_init(TC_X509_policy_reader* reader, TC_bytes value,
                                    const TC_TLV_limits* limits, TC_bytes* seen, size_t capacity);
TC_TLV_result TC_X509_policy_next(TC_X509_policy_reader* reader, TC_X509_policy* out);
#endif

typedef struct {
  TC_bytes oid, value;
} TC_X509_policy_qualifier;
#if TC_ENABLE_X509
/* Pass policy.qualifiers, including {NULL, 0} when absent, which returns END
 * from the first next. Each PolicyQualifierInfo (RFC 5280 section 4.2.1.4)
 * holds an OID and one value. value retains its DER tag and length and
 * counts as one element. OID-specific syntax and acceptance are caller
 * checks. */
TC_TLV_result TC_X509_policy_qualifiers_init(TC_TLV_reader* reader, TC_bytes qualifiers,
                                             const TC_TLV_limits* limits);
TC_TLV_result TC_X509_policy_qualifier_next(TC_TLV_reader* reader, TC_X509_policy_qualifier* out);
#endif

typedef struct {
  int ca, has_path_length;
  uint32_t path_length;
} TC_X509_basic_constraints;
/* DistributionPointName wrapper and its GeneralNames or relative-RDN
 * contents. */
typedef struct {
  TC_bytes encoded, contents;
  int relative;
} TC_X509_distribution_name;
#if TC_ENABLE_X509
/* BasicConstraints (RFC 5280 section 4.2.1.9). An explicit cA must be TRUE
 * because DER omits the DEFAULT, and pathLenConstraint requires cA, otherwise
 * the result is INVALID. A path length above UINT32_MAX returns LIMIT. The
 * empty sequence gives ca 0. */
TC_TLV_result TC_X509_basic_constraints_read(TC_bytes value, const TC_TLV_limits* limits,
                                             TC_X509_basic_constraints* out);
#endif
/* Mask bit n is ASN.1 KeyUsage bit n, independent of encoded byte order. */
enum {
  TC_KEY_USAGE_DIGITAL_SIGNATURE = 1u << 0,
  TC_KEY_USAGE_CONTENT_COMMITMENT = 1u << 1,
  TC_KEY_USAGE_KEY_ENCIPHERMENT = 1u << 2,
  TC_KEY_USAGE_DATA_ENCIPHERMENT = 1u << 3,
  TC_KEY_USAGE_KEY_AGREEMENT = 1u << 4,
  TC_KEY_USAGE_CERT_SIGN = 1u << 5,
  TC_KEY_USAGE_CRL_SIGN = 1u << 6,
  TC_KEY_USAGE_ENCIPHER_ONLY = 1u << 7,
  TC_KEY_USAGE_DECIPHER_ONLY = 1u << 8,
  TC_KEY_USAGE_ALL = (1u << 9) - 1
};
#if TC_ENABLE_X509
/* KeyUsage (RFC 5280 section 4.2.1.3) as a combination of the masks above.
 * At least one bit must be set, and the DER named-bit rules apply: no
 * trailing zero bits and at most nine bits. Violations return INVALID. */
TC_TLV_result TC_X509_key_usage_read(TC_bytes value, const TC_TLV_limits* limits, uint16_t* out);

/* Decode the nonempty sequence of key-purpose OIDs inside an EKU extension
 * (RFC 5280 section 4.2.1.12). oids receives each OID's contents, and
 * capacity counts span slots. Input, oids and count must be disjoint, and
 * overlap returns ARGUMENT. More OIDs than capacity return LIMIT. On failure
 * neither output changes. Unknown purpose OIDs are retained. */
TC_TLV_result TC_X509_extended_key_usage_read(TC_bytes value, const TC_TLV_limits* limits,
                                              TC_bytes* oids, size_t capacity, size_t* count);
#endif

typedef struct {
  int has_require_explicit_policy, has_inhibit_policy_mapping;
  uint32_t require_explicit_policy, inhibit_policy_mapping;
} TC_X509_policy_constraints;
#if TC_ENABLE_X509
/* PolicyConstraints (RFC 5280 section 4.2.1.11). At least one field must be
 * present, otherwise the result is INVALID. Counts above UINT32_MAX return
 * LIMIT. A present zero count applies immediately. An absent field adds no
 * constraint. */
TC_TLV_result TC_X509_policy_constraints_read(TC_bytes value, const TC_TLV_limits* limits,
                                              TC_X509_policy_constraints* out);
#endif
/* InhibitAnyPolicy is a nonnegative INTEGER. Decode it with TC_DER_uint32. */

#ifdef __cplusplus
}
#endif
#endif
