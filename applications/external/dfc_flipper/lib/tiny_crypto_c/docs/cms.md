<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# CMS parsing and signature verification

Include `<tiny_crypto/cms.h>` and enable `TINY_CRYPTO_ENABLE_CMS`.
This base layer parses metadata, hashes content, and verifies signatures with a
caller-selected public key. It depends on X.509 key parsing, BER framing, and
the small PIV/TWIC identifier classifier used by the supported signed attributes.

Signer discovery, certificate paths, and revocation are an optional layer.
Include `<tiny_crypto/cms_validation.h>` and enable
`TINY_CRYPTO_ENABLE_CMS_VALIDATION` for those operations. This also requires the
X.509 path and revocation features.

Pass message-specific inputs in `TC_CMS_validation_request`: the encoded CMS
object, zero-based signer index, expected content-type OID contents, and any
detached content spans. Attached messages use a zero detached span count.
For detached messages, spans are hashed in array order; zero spans represent
an empty message. Keep the request, span array and backing bytes stable and
separate from writable scratch for the duration of validation.

Set `signer_certificate` to a borrowed DER certificate when the application
requires a particular signer. The validator checks its SignerInfo identity,
signature, certificate path and revocation status. Other message and source
certificates remain available as issuers. Leave this field `{NULL, 0}` to
discover the signer from the message and certificate source.

PIV biometric objects that omit their signing certificate use the authenticated
CHUID signing certificate. Keep that certificate's backing buffer stable while
validating the biometric signature.

`TC_PIV_biometric_validate` in `<tiny_crypto/credential.h>` provides this
workflow. Pass a complete CBEFF `BC` value, the authenticated CHUID's FASC-N and
GUID, its signing certificate, and the validated card certificate's expiration.
The operation checks header metadata, binds the header and signed identifiers,
and validates the CMS signature, signer path and revocation. It shares the
CHUID operation's content-signing policy and caller-owned context/workspace.
An embedded biometric certificate must carry a different signing key from
CHUID. This comparison uses RSA modulus/exponent or the named EC curve and
point, including compressed/uncompressed representations. Reissued certificates
with the same key follow the certificate-omission rule too.

Decrypt any outer TWIC privacy-key wrapping before calling this operation. Keep
the CHUID, biometric and certificate buffers stable through the acceptance
decision. A successful result authenticates the biometric object. The application
still selects acceptable biometric formats and dates, validates record contents,
and matches a physical sample. The reader command can authenticate the encrypted
TWIC biometric objects using its `--tpk-hex` option.

## SignedData envelopes

For PIV signed objects, include `<tiny_crypto/piv_cms.h>` and call
`TC_PIV_CMS_read` with `TC_PIV_CMS_CHUID` or `TC_PIV_CMS_BIOMETRIC`.
It checks the external-signature layout and required attributes from
SP 800-73-5 Part 1 and SP 800-76-2. Choose the OID namespace policy and
attribute encoding explicitly. `TC_PIV_CMS_BIOMETRIC` requires both FASC-N
and entryUUID. Select `TC_PIV_CMS_BIOMETRIC_LEGACY` explicitly for the
FIPS 201-1 signature profile: FASC-N remains mandatory and a present entryUUID
must match CHUID. Use the same profile for identifier matching and set
`TC_PIV_biometric_validation_request.signature_profile` when validating it.
The reader also checks the selected digest's parameters and membership in
`digestAlgorithms`. Unknown selected hashes return `TC_TLV_UNSUPPORTED`.

For a biometric object's `BC` value, `TC_PIV_CBEFF_read` returns the biometric
record, CMS signature, header FASC-N and complete signed-content span. It checks
the version and block lengths without copying payload bytes. Pass `signature`
to `TC_PIV_CMS_read` and `signed_content` to content verification. Biometric
format, date, quality and encryption metadata need application checks.

`TC_PIV_CBEFF_metadata_read` checks the header's calendar values, date ordering,
security options, quality range, creator text and reserved bytes. It returns
format identifiers and the encryption flag for application handling. Creation,
valid-from and valid-until values use `TC_X509_time`. No current time is supplied;
the caller decides how biometric validity dates affect its workflow.

`TC_PIV_CBEFF_format_identify` recognizes the fingerprint image, fingerprint
template, iris image and facial image header tuples in SP 800-76-2 Table 15.
It checks the owner, format, biometric type and processing bits together.
Use the result to select a record reader; validate the record and handle any
encryption before using its biometric data.

TWIC Part 2 Appendix G requires checking fingerprint minutiae before interpreting
header quality. A quality of `-2` can accompany usable templates. Applications
must inspect the ANSI/INCITS 378 record to decide whether matching is possible.

TWIC's enciphered fingerprint `BC` value wraps the complete CBEFF object in
TPK encryption and PKCS#7 padding (Part 2, section 4.6.4). Decrypt and validate
that padding before passing the recovered CBEFF bytes to these readers. The
header's encryption flag describes the biometric data block inside CBEFF.

The result borrows the envelope, signer, attributes and optional certificate.
Pass the certificate to `TC_X509_read`, then check signer identity, usage,
policy and trust alongside the content signature. A biometric object without
an embedded certificate must use the CHUID signing key. Match its FASC-N and
UUID against the credential before accepting it.

`TC_PIV_CMS_identifiers_match` compares the object's signed FASC-N and entryUUID
with the credential's 25-byte FASC-N and 16-byte CHUID GUID. Pass the same stable
buffers and `TC_PIV_CMS_kind` used by the reader. Biometric signatures require
both attributes. CHUID signatures permit either attribute to be absent; each
present attribute must match. Also bind the CHUID's signed FASC-N and GUID to
the card-authentication certificate using `TC_PIV_card_identifiers_match`.
The attribute comparison scans OCTET STRING chunks and writes `matched`
only on success. A mismatch writes zero; malformed data or exhausted limits
return an error. Signature and trust validation are also required for acceptance.

`TC_CMS_signed_data_read` reads a complete ContentInfo carrying SignedData.
It checks envelope framing, digest AlgorithmIdentifier syntax, and SignedData
version consistency. Enable `TINY_CRYPTO_TLV_BER`: CMS envelopes allow both
definite and indefinite BER lengths. This does not select the signed-attribute
compatibility mode described below.

The returned spans borrow the original buffer. `content_type` contains the OID
contents; the other fields retain their complete encodings. `content` is an
OCTET STRING and may contain nested chunks, so it is not necessarily a contiguous
payload. `has_content` distinguishes detached content from an embedded empty
value. Certificate, revocation and signer collections need further parsing.

[The fixed-workspace example](../examples/cms_reader.c) accepts envelopes up to
16 KiB and returns parsing or resource-limit errors to its caller. Its
[header](../examples/cms_reader.h) supports C and C++ callers. Compile the source
alongside your application and link `tiny_crypto_c::tiny_crypto_c`.

Keep the input alive and unchanged while using the result. The frame workspace
can be reused after the call. Input, limits, frames, work counter and output
must not overlap. Bad storage arguments return `TC_TLV_ARGUMENT` before changing
caller storage; other failures may consume work and scratch but leave output
unchanged. `TC_TLV_LIMIT` means a configured bound was reached, not that the
message is valid or invalid.

## SignerInfo

`TC_CMS_signer_info_read` reads one complete SignerInfo, not the surrounding
signerInfos SET. Select `TC_TLV_BER` for CMS framing or `TC_TLV_DER` when the
application requires DER. Neither choice enables the signed-attribute
compatibility mode.

Version 1 identifies the signer by issuer Name and certificate serial number;
version 3 uses a subject key identifier. The reader checks that the identifier
matches the version. Names and algorithm identifiers receive syntax checks,
but an unknown algorithm is not rejected just because the library cannot use it.

`issuer` retains its Name encoding. `serial` contains INTEGER bytes, including
sign padding. `subject_key_id` and `signature` retain their complete OCTET STRING
encodings, including any BER chunks. Decode those strings before passing their
contents to a cryptographic API. Signed and unsigned attributes need separate
checks. An unsigned attribute is not protected by the SignerInfo signature.

The fixed-workspace example also provides `example_read_cms_signer`. It uses BER
framing and the same bounds and borrowed-buffer lifetime as the envelope reader.

To read a collection, pass `signed_data.signers` to `TC_CMS_signers_init`, then
call `TC_CMS_signer_next` until it returns `TC_TLV_END`. Keep one work counter
across those calls. Initialization checks the SET's BER framing; each next call
checks one SignerInfo. The reader position and output stay unchanged on failure.
An exhausted reader returns `END` even when its work budget is zero.

The [example loop](../examples/cms_reader.c) in `example_parse_cms_signers`
propagates errors from any member, rather than treating an early stop as success.
Reader state, input bytes, frames, work counter and output must not overlap.
Keep the input bytes stable while iterating or using returned spans. SignedData
may contain no signers; parsing an empty collection does not authenticate it.

## RSA parameter compatibility

The ordinary signature-verification functions require a `NULL` parameter for
CMS `rsaEncryption`, as specified in RFC 3370 section 3.2. Some captured PIV
biometric signatures omit that parameter. Applications can accept those with
`TC_CMS_signer_verify_content_with_policy` or
`TC_CMS_signer_verify_digest_with_policy`:

```c
TC_CMS_verification_policy policy = {
    TC_CMS_ATTRIBUTES_DER,
    TC_CMS_RSA_PARAMETERS_ALLOW_ABSENT
};
```

For path construction and credential validation, set
`TC_CMS_path_options.rsa_parameters` to the same value. Zero-initialized options
require `NULL`. Present parameters must still encode `NULL`; certificate
algorithms and RSA signature padding retain their existing checks. Signed
attribute ordering is selected separately through `attributes`.

## Signed attributes

Pass the complete `SignerInfo.signedAttrs` field, including its implicit `[0]`
tag, to `TC_CMS_signed_attributes_read`.

Pass `TC_CMS_ATTRIBUTES_DER` for RFC 5652 encoding. An application that needs
TWIC/MyID compatibility can select `TC_CMS_ATTRIBUTES_BER_DEFINITE_ORDER`
instead, with `TINY_CRYPTO_TLV_BER=ON`. The library does not detect cards or
retry verification in another mode.

The compatibility mode accepts definite BER lengths and preserves attribute
order. It still rejects indefinite lengths, duplicate required attributes,
missing content-type or message-digest, and malformed values. Optional
signingTime, SMIMECapabilities and pivSigner-DN are accepted; other attributes
currently return `TC_TLV_UNSUPPORTED`.

```c
#include <tiny_crypto/cms.h>

TC_TLV_result read_attributes(TC_bytes input,
    TC_CMS_attribute_encoding encoding, TC_CMS_signed_attributes *attributes)
{
    const TC_TLV_limits limits = {4096, 4096, 64, 8};
    TC_TLV_frame frames[8];
    size_t work = 16384;
    return TC_CMS_signed_attributes_read(input, encoding, &limits,
        frames, 8, &work, attributes);
}
```

Only use `attributes` after `TC_TLV_OK`. Its spans borrow the input buffer,
which must remain alive and unchanged. The frame array is temporary scratch.
Input, limits, frames, work counter and output must occupy separate storage.
Choose limits for the largest object your application accepts.

`smime_capabilities` borrows the encoded capability sequence, including its tag
and length. The reader checks the single attribute/value requirement and each
capability's OID with at most one parameter value, following
[RFC 8551 section 2.5.2](https://datatracker.ietf.org/doc/html/rfc8551#section-2.5.2).
Unknown capability identifiers and their parameters remain opaque. The list
preserves the signer's preference order; it has no effect on signature algorithm
selection or local policy. Interpret advertised capabilities only after trust
and signature validation.

`signer_name` borrows the Name encoded in `pivSigner-DN`
(`2.16.840.1.101.3.6.5`). The reader validates its X.509 name structure and
requires a single attribute value. CMS path building also matches this name
against the candidate certificate's subject, using the shared name-comparison
rules. The signature-only APIs accept a public key; callers using those APIs
must perform the certificate/name binding themselves. A PIV CHUID profile must
require this attribute in addition to the general CMS checks.

`fascn_oid` preserves the PIV or TWIC FASC-N identifier. Apply the application's
namespace policy with `TC_PIV_oid_identify`. `fascn_octets` borrows the encoded
OCTET STRING, including its tag and length. The reader requires 25 value bytes
and rejects duplicate FASC-N attributes, including a PIV/TWIC pair. DER requires
a primitive value; the explicit BER mode also accepts definite-length chunks.
Use the TLV reader for a primitive value or walk the primitive OCTET STRING
chunks in order for a fragmented value. Compare the authenticated identifier
with the credential's CHUID FASC-N before relying on the binding.

`entry_uuid_octets` borrows the encoded `entryUUID` value
(`1.3.6.1.1.16.4`). Its ASN.1 syntax is a 16-byte OCTET STRING, as specified in
[RFC 4530 section 2.1](https://www.rfc-editor.org/rfc/rfc4530.html#section-2.1).
It uses the same DER/BER chunk handling and duplicate checks as FASC-N.
Credential validation must bind this authenticated value to the CHUID's card UUID.

Before trusting the content, compare its type and computed digest with the
attributes, verify the signature, and validate the signer's certificate path
and permitted usage. Hash the two `signature_input` spans in order when
verifying: they substitute the SET OF tag while preserving the original
length and content bytes. Do not sort or re-encode them in compatibility mode.

## PIV and TWIC identifiers

`<tiny_crypto/piv_oid.h>` provides `TC_PIV_oid_identify` for OID contents.
Select `TC_PIV_OIDS_ONLY` for PIV identifiers or
`TC_PIV_OIDS_TWIC_COMPATIBLE` to accept the PIV/TWIC pairs listed in
[TWIC Part 2 v5, section 6](https://www.tsa.gov/sites/default/files/twic-nexgen-_-legacy-part-2-card-specification-v5.pdf).
The choice belongs to the application and applies to objects from either card
application. The helper is available with X.509 support.

The result identifies certificate-policy, FASC-N, content-signing, card-authentication
and background-check OIDs. PIV CHUID content and signer-DN are recognized under
their PIV OIDs; the TWIC table supplies no corresponding aliases. The table's
TWIC digital-signature policy is recognized in compatibility mode.
Unknown, malformed and disabled identifiers return `TC_PIV_OID_UNKNOWN`.
Keep the original OID bytes for signature verification and certificate-path
policy processing. Recognition alone establishes no trust or permitted usage.

## Content digest binding

### CHUID content

For a signed CHUID, `TC_PIV_CHUID_read_profile` returns two borrowed
`signed_content` spans. Hash them in order with the CMS signer's digest
algorithm, then pass the digest to `TC_CMS_signer_verify_digest`. The spans
cover the encoded CHUID fields before and after the `3E` signature field,
including the trailing `FE 00`. They exclude the outer `53` response wrapper.
Keep the original tag and length bytes when hashing, including nonminimal
length encodings accepted by the CHUID reader.

`TC_CMS_signed_data_path_build_parts` accepts this array directly as detached
content. Pass `chuid.signature` as the envelope and `chuid.signed_content` with
a count of two. It selects the signer's hash, checks the envelope and content
binding, and builds the certificate path with the same options and workspace
as `TC_CMS_signed_data_path_build`. CHUID profile requirements and revocation
checks still belong to the credential-validation workflow.

The combined example exposes `example_validate_cms_credential` for a held
snapshot and `example_validate_cms_from_store` for a store. Both accept a
`TC_CMS_validation_request` and carry its ordered detached spans through
signature/path validation and the selected path's revocation check:

```c
TC_CMS_validation_request request = {
    chuid.signature, 0, expected_content_type, chuid.signed_content, 2, {NULL, 0}
};
TC_credential_status status = example_validate_cms_credential(
    &request, snapshot, &options, &revocation, &work, &workspace);
```

For attached content, set `detached_content` to NULL and `detached_count` to zero.
Keep the request, spans, source buffers and CRL index stable until the acceptance
decision. Apply the CHUID profile and identifier checks described above.

The native CHUID tests generate their own issuing CA, content-signing certificate
and CRL. They cover a valid signer path, signer revocation, an unrelated trust
key and changed detached content, with optional PIV/TWIC identifier attributes.
Validly signed objects naming the wrong signer are rejected during credential
validation. Both registered content-signing purpose OIDs are exercised under
TWIC policy; the PIV profile accepts its registered namespace. Unsigned TWIC
objects are checked against their parsing schema before testing rejection by
the signed-object validator.

### Validate a CHUID against a card certificate

`TC_PIV_CHUID_validate` in `<tiny_crypto/credential.h>` combines the signed
CHUID checks. First validate the slot 9E certificate and read
its identifiers with `TC_PIV_card_identifiers_read`. Hold the certificate buffer,
CHUID buffer, trust source and CRL index through the decision.

```c
TC_PIV_CHUID_validation_request request = {
    encoded_chuid, TC_PIV_CHUID_CONTAINER, TC_TWIC_NEXGEN_CARD,
    &card_identifiers, &card_certificate.not_after
};
TC_CMS_path_workspace path_workspace = /* caller-owned arrays and capacities */;
TC_CMS_credential_workspace workspace = {
    &path_workspace, held_path, path_capacity,
    crl_states, crl_capacity, revocation_nodes, node_capacity
};
TC_validation_trust trust = { &snapshot->source, &crl_index };
TC_validation_context context;
if (TC_validation_context_init(&trust, &validation_options,
        &workspace, &context) != TC_RESULT_OK) {
    return TC_CREDENTIAL_ERROR;
}
TC_PIV_CHUID_result accepted;
TC_credential_status status = TC_PIV_CHUID_validate(
    &request, &context, &work, &accepted);
if (status != TC_CREDENTIAL_VALID) {
    /* Report the validation outcome and end this credential check. */
    return status;
}
```

The operation uses one `TC_validation_options` value for time, provider,
certificate policy, CRL-signer policy, limits and CMS compatibility. It checks
the CHUID's signed FASC-N/GUID against the
certificate, checks any corresponding signed attributes, requires content-signing
key usage and purpose, and verifies the signature, path and revocation status.
Scratch is provisional and remains caller-owned; clear it when its lifetime ends.
On success, `accepted` borrows the authenticated CHUID fields and signer
certificate for dependent Security Object and biometric checks. Keep their
backing bytes unchanged while those values are used.

For `TC_PIV_CARD`, the operation selects the registered PIV content-signing initial
certificate policy, requires that exact policy in the signer certificate, and
requires the signer certificate to remain valid through
the card certificate's expiration. TWIC profiles retain the caller's certificate
policy settings and accept either registered PIV/TWIC content-signing purpose.
An empty `validation_options.certificate.purpose` asks the validator to select
the signer's one exact compatible content-signing EKU. An explicit value
requires that exact OID.
Attribute encoding remains an explicit CMS option.

This example treats the CHUID's expiration date as valid through 23:59:59 UTC on
that date. Its signed-object workflow rejects unsigned CHUIDs. Card cancellation,
active-card authentication and access authorization require their own checks.

These examples call `TC_CMS_credential_validate`, which combines signature,
path and revocation validation. Its workspace adds a retained path-span array,
one cache byte per CRL and a caller-sized revocation-node array to the CMS path
workspace. It checks both phases' input and scratch ranges before processing,
and checks borrowed source records as callbacks return them. Only
`TC_CREDENTIAL_VALID` establishes a valid, unrevoked signer path;
`TC_CREDENTIAL_REVOKED` distinguishes certificate revocation from other failures.
The application supplies a stable source, a CRL index and two policies using
the same validation time. Revocation uses the selected CMS trust anchor.

Both spans are empty for unsigned TWIC CHUIDs. Applications must select the
unsigned profile explicitly and obtain credential authentication through their
chosen TWIC validation workflow.

`TC_CMS_content_digest` hashes `signed_data.content` directly from its BER
encoding, including nested definite or indefinite chunks. Enable BER and the
selected hash implementation. The function uses one temporary hash context on
the stack, with caller-owned parsing frames and no message-sized copy.

Pass a digest buffer large enough for the selected hash. Success writes only
the digest bytes; failure leaves the buffer unchanged. Input, limits, frames,
work counter and the entire output buffer must occupy separate storage.
Reuse the envelope reader's frames after parsing, and keep one work counter
across operations to bound the combined processing cost.

`TC_CMS_content_digest_check` compares parsed signed attributes with a computed
digest and the SignedData content type. Pass OID contents, not its tag and length.
Hash content value bytes only: OCTET STRING headers, chunk headers and end markers
are not part of the message. Detached content comes from the application.

The hash algorithm must match the SignerInfo digest algorithm. Compute each
distinct digest once and reuse it across signers. The digest may come from the
library or an external provider; this comparison does not require that hash to
be enabled in the build.

Check both the return status and `matched`. `TC_TLV_OK` with `matched == 0` means
the content type or digest differs. Errors preserve `matched`; `TC_TLV_LIMIT`
must not be treated as a mismatch or a match. A successful match still requires
signature verification and signer trust validation before accepting the content.

Inputs may share a buffer. Keep the work counter and result separate from each
other and from every input, including the signed-attribute view itself.

## Verify a signer

`TC_CMS_signer_verify_content` hashes content and verifies one parsed SignerInfo
with a supplied public key. It selects the content and signature hashes from the
signer's algorithm identifiers. Use `TC_CMS_CONTENT_BER_OCTETS` with
`signed_data.content`, or `TC_CMS_CONTENT_RAW` for application-provided message
bytes. Raw NULL/0 is an empty message. The function does not detect the format or
retry malformed BER as raw content.

Raw content is bounded by `limits.max_input` and `limits.max_value`. Encoded
content also uses the framing limits. Both paths charge hashing to the shared
work budget and reuse one hash context and digest buffer for content and signed
attributes. No message-sized copy is made.

Use `TC_CMS_signer_verify_digest` for a cached or externally computed digest.
Compute it with the signer's `digest_algorithm`. This lets multiple signers reuse
a digest instead of hashing the message again. PSS may use a different hash for
signed attributes; both verification APIs handle that distinction internally.

Pass the envelope's `content_type` and select the signed-attribute encoding
explicitly. The verifier checks attribute binding and hashes the original signed
bytes, then calls the provider's digest operation. Without signed attributes,
only `id-data` content is allowed. This API is not for countersignatures.

`TC_CMS_signature_workspace` holds caller-owned parser frames and optional
signature storage. A primitive signature is borrowed directly. Fragmented BER
signatures are joined in that storage, so size it for the signature, not the
message. Insufficient capacity returns `TC_X509_SIGNATURE_LIMIT`.

The compiled [examples](../examples/cms_reader.c), `example_verify_cms_content`
and `example_verify_cms_digest`, use 16 frames and a 384-byte signature buffer for
RSA through 3072 bits and supported ECDSA signatures. They propagate verification
errors and accept an explicit work budget and attribute encoding. Use a separate
native-provider workspace as shown in [signature verification](x509-crypto.md).

Only `TC_X509_SIGNATURE_VALID` is a successful signature check. It does not
establish that the key belongs to the claimed signer or is trusted. The caller
must select the signer certificate, validate its path and usage, apply revocation
and algorithm policy, and enforce envelope-level requirements. This operation
does not check membership in the envelope's `digestAlgorithms` collection.

Keep every input and metadata object stable during verification. Frames,
signature storage, provider scratch and the work counter must be separate from
each other and from those inputs. Parsing scratch may change on failure.

## Find a signer path

The APIs in this section come from `<tiny_crypto/cms_validation.h>`. Applications
that only parse or verify CMS signatures can omit this header and feature.

`TC_CMS_signer_path_build` selects a certificate by issuer/serial or subject key
identifier, verifies the CMS signature against a supplied content digest, and
constructs a path to an application-provided trust anchor. Pass a parsed
SignerInfo, the envelope's content type, the computed digest and
`signed_data.certificates`. Use `{NULL, 0}` when certificates are external only.

The source supplies additional candidates and explicit trust anchors. Embedded
certificates can supply the signer and intermediates; their presence does not
make them trust anchors. Hold the source snapshot and its returned bytes stable
throughout the call and while using the result. No network fetching occurs.

`TC_CMS_path_options.path` takes the same time, usage, policy, provider and path
limits as [X.509 path validation](x509-path.md). Set these for the object being
authenticated. `attributes` selects DER or the explicit BER compatibility mode.
`max_candidates` bounds all embedded choices plus external candidates;
`max_candidate_bytes` bounds their combined encoding size, including embedded
collection framing. The supplied work counter covers indexing and all candidate
attempts; `path.max_work` is not used by this call.

`TC_CMS_path_workspace` combines validation/search scratch, a certificate-span
index and optional fragmented-signature storage. Size the index for every X.509
candidate, not just the expected signer. Other certificate formats consume the
record and byte limits but need no index slot. The index borrows bytes; external
candidates are fetched once. Parsing frames and name scratch are reused between
CMS and path processing. Keep all writable arrays separate from input bytes,
metadata, provider state and each other.

The compiled `example_find_cms_signer_path` in
[cms_validate.c](../examples/cms_validate.c) provides storage for eight certificates,
four path entries and signatures through RSA-3072. It shares the
[workspace setup](../examples/x509_workspace.h) with the X.509 examples.
Allocate `ExampleCMSPathWorkspace` outside a small task stack, pass application
options and a work budget, and check for `TC_X509_PATH_VALID` before using the
result. Size limits remain application choices; larger inputs return `LIMIT`.

On success, `result.path` holds the selected path in anchor-issued-first order,
with the signer last. `anchor_index` identifies the source trust anchor.
`validation.work_used` includes indexing and unsuccessful attempts. Keep the
input bytes, source snapshot, search path and policy output alive while using
the result. Reusing the workspace invalidates its path and policy outputs.

Invalid candidate signatures and paths allow another candidate to be tried.
Source errors and provider errors stop discovery; unresolved algorithms and
resource limits retain their own status. Failures preserve the result object,
though scratch and work may be consumed after storage preflight.

The application supplies the SignedData policy for this prehashed operation,
including revocation, acceptable algorithms, signer membership
and the envelope's `digestAlgorithms`. Compute the supplied digest with the
Signer's `digest_algorithm`; cached digests can be reused across candidates.

## Validate a SignedData signer

`TC_CMS_signed_data_path_build` accepts the complete encoded envelope and handles
parsing, content hashing, signer selection and path construction. Use it when
the application has the whole CMS object and does not need cached digests.
It uses the same options, workspace and trust source as the prehashed path API.

Pass the expected content type as OID contents. A different envelope content
type returns `TC_X509_PATH_INVALID`. `signer_index` selects a zero-based member
of `signerInfos` in encoded order. The function checks every signer's schema
while checking the envelope version, but authenticates only the selected signer.
An absent index returns `INVALID`; an empty signer set cannot succeed.

For attached content, pass `{NULL, 0}` as `detached_content`. The function hashes
the envelope's OCTET STRING value, excluding BER headers and chunk markers.
Supplying replacement bytes for an attached envelope returns `ERROR`. For a
detached envelope, pass the raw application message; `{NULL, 0}` represents an
empty message. Malformed attached content is never retried as detached content.

The selected signer's digest must appear in `digestAlgorithms`. This is the
validation policy permitted by [RFC 5652 section 5.1](https://www.rfc-editor.org/rfc/rfc5652.html#section-5.1).
SHA identifiers accept absent or NULL parameters, including BER NULL encodings;
the comparison does not require identical encoded bytes. Unknown algorithms for
other signers are allowed. An unknown or disabled selected hash returns
`UNSUPPORTED`. The content hash comes from `digestAlgorithm`, even when PSS uses
a different hash for signed attributes.

Content is hashed once before trying candidate certificates. A temporary context
hashes the content; the resulting digest is wiped after path construction.
No encoded content or certificate copies are required. The shared work counter
covers the complete operation, including unsuccessful candidate attempts.

The compiled `example_check_cms_signed_data` in
[cms_validate.c](../examples/cms_validate.c) initializes the workspace and propagates
the result. Supply application-specific time, usage, policy and providers in
`options.path`, candidate bounds in `options.max_candidates` and
`options.max_candidate_bytes`, and the explicit attribute encoding in
`options.attributes`. Keep the source snapshot and result storage alive until
you finish using the returned signer path.

Only `TC_X509_PATH_VALID` confirms the selected signature and certificate path.
The application still applies revocation and acceptable-algorithm policy. If a
protocol requires multiple signatures, validate each required signer explicitly;
a successful call does not authenticate the other signers.

## Credential validation example

`example_validate_cms_credential` in [cms_validate.c](../examples/cms_validate.c)
combines SignedData signature/path validation with required CRL checking.
Acquire a trust-store snapshot before calling it and release it after the
acceptance decision. Keep the CRL index and its backing bytes stable throughout.
Serialize store acquisition and release with the application's lock.

Supply the expected content type, signer index, signed-attribute encoding,
holder usage policy and `TC_CMS_revocation_policy` explicitly. The CMS policy
contains the CRL index, CRL-signer path policy, candidate-byte limit, delta
policy, and ordering policy. Both path policies must use the same validation
time. The validator binds CRL checks to the held certificate source and the
anchor selected during path construction. Additional CRL signer certificates
must be available from that source. Include the root certificate as a candidate
when it signs a CRL; the separately configured trust anchor establishes trust,
while the candidate supplies the CRL signer's certificate and extensions.

Allocate `ExampleCMSCredentialWorkspace` outside a small task stack. It retains
four path spans while reusing CMS search scratch for revocation, with capacity
for four CRLs and eight certificate dependencies. Certificate bytes remain
borrowed. One caller-owned work counter bounds both phases.
Missing index storage, invalid revocation policy values and an index exceeding
the example's CRL capacity are rejected before hashing or signature checks.

`example_validate_cms_from_store` handles acquisition and release around this
workflow. Hold the application store/source lock through the call and the final
acceptance decision. It returns `TC_CREDENTIAL_UNAVAILABLE` when no
snapshot is published and releases each acquired snapshot on every return path.
The returned status contains no borrowed certificate views. The workspace is
scratch and can be reused once the call returns.

Only `TC_CREDENTIAL_VALID` confirms the selected signature, path and
unrevoked status. Revoked credentials return `TC_CREDENTIAL_REVOKED`;
missing CRL evidence returns `TC_CREDENTIAL_UNSUPPORTED`. Invalid,
resource-limit and operational failures have separate results. Apply the
protocol's object profile, credential identifiers and access policy separately.

### Command-line example

[cms_check.c](../examples/cms_check.c) loads a SignedData object, an explicitly
trusted root, one intermediate certificate, and their two complete CRLs. It
checks signer zero with the native crypto provider at the supplied UTC time.
The expected content type is `id-data`; the signer must have digital-signature
key usage. Supply DER files and an attached CMS payload.
Supported signed attributes are content type, message digest, signing time,
S/MIME capabilities, pivSigner-DN, PIV/TWIC FASC-N and entryUUID.
Other attributes return `unsupported`.

Build against an installed library with X.509, EC and RSA enabled:

```sh
cmake -S . -B build-library -DTINY_CRYPTO_RESOURCE_PROFILE=desktop \
  -DTINY_CRYPTO_BUILD_TESTS=OFF -DTINY_CRYPTO_BUILD_BENCHMARKS=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/build-install"
cmake --build build-library
cmake --install build-library
cmake -S examples/cms_check -B build-cms-check \
  -DCMAKE_PREFIX_PATH="$PWD/build-install"
cmake --build build-cms-check
./build-cms-check/cms_check signed-data.der root.der issuer.der \
  root.crl issuer.crl 20260908120000
```

Exit status is zero for valid signature, path and revocation evidence, one for
a validation failure, and two for input or setup errors. The root file is a
trust decision made by the caller. Provision it through an authorized channel.

The example reserves static buffers for a 16 KiB CMS object, two 4 KiB
certificates and two 16 KiB CRLs. Its RSA workspace supports keys through
3072 bits. Adapt these capacities and the object-specific policy to your
application; PIV object validation also requires its prescribed content type
and field bindings.

Run CMS checks with:

```sh
ctest --test-dir build -R '^test_cms_(reader|attributes|signer_info|content|verify|verify_content|pss|native)$' --output-on-failure
```
