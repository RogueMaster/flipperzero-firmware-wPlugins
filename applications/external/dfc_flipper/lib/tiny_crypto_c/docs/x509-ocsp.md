<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# X.509 OCSP

Include `<tiny_crypto/x509_ocsp.h>` and enable `TINY_CRYPTO_ENABLE_X509_OCSP`.
OCSP requires `TINY_CRYPTO_ENABLE_X509_PATH` and SHA-1, because a byKey
ResponderID is always a SHA-1 key hash (RFC 6960 section 4.2.1).

The module has two operations for one certificate at a time:

- `TC_X509_ocsp_request_encode` writes an unsigned DER OCSPRequest
  (RFC 6960 section 4.1.1) with an optional RFC 9654 nonce.
- `TC_X509_ocsp_response_verify` authenticates a complete DER OCSPResponse,
  including a stapled one, and reports GOOD or REVOKED.

The caller validates the certificate path first, generates the nonce, moves the
request and response over HTTP (RFC 6960 appendix A) and makes the final
acceptance decision. `TC_X509_path_check_revocation` applies OCSP responses to
a whole path with CRL fallback. See [path revocation](x509-revocation.md).

## Quick start

[examples/x509_ocsp.c](../examples/x509_ocsp.c) wraps both calls with fixed
limits and the [shared X.509 workspace](../examples/x509_workspace.h). Fill the
nonce from a DRBG or another approved random source.

```c
static ExampleX509Workspace storage;
uint8_t nonce[EXAMPLE_OCSP_NONCE_LENGTH];
uint8_t request[EXAMPLE_OCSP_REQUEST_CAPACITY];
size_t request_length = 0;
if (example_ocsp_request(certificate, &issuer, nonce, &storage,
                         (TC_buffer){request, sizeof request},
                         &request_length) != TC_TLV_OK)
    return EXAMPLE_OCSP_ERROR;
/* POST request[0..request_length) and read the response. */

ExampleOcspCheck check;
memset(&check, 0, sizeof check);
check.certificate = certificate;
check.issuer = &issuer;
check.nonce = (TC_bytes){nonce, sizeof nonce};
check.response = response;
check.at = now;
check.verifier = &verifier;
TC_X509_ocsp_report result;
switch (example_ocsp_check(&check, &storage, &result)) {
case EXAMPLE_OCSP_GOOD:
    return accept(&result);
case EXAMPLE_OCSP_REVOKED:
    return reject_revoked(result.revocation_time, result.reason);
case EXAMPLE_OCSP_CHECK_RESPONDER:
    /* Check result.responder_certificate against a CRL first. */
    return check_responder(result.responder_certificate);
default:
    /* NO_DECISION, REJECTED, LIMIT or ERROR: try CRLs or fail closed. */
    return use_crls();
}
```

`certificate` is the DER certificate and `issuer` is the
`TC_X509_trust_anchor` of its issuer, taken from the validated path.
`verifier` is a `TC_X509_signature_provider`, such as
`TC_X509_native_provider`. The example wipes its storage and zeroes `result`
after every verification failure.

## Encoding a request

Set `TC_X509_ocsp_encode_request`:

- `certificate`: the DER certificate to check.
- `issuer`: the name and public key of its issuer. The certificate's issuer
  Name must match `issuer->name`.
- `hash`: the CertID hash. Use `TC_HASH_SHA256`, or `TC_HASH_SHA1` for a
  legacy responder. Other hashes return `TC_TLV_UNSUPPORTED`.
- `nonce`: empty, or 32 to 128 caller-generated random bytes (RFC 9654
  section 2.1).
- `parsing`: limits for parsing the certificate.

The workspace supplies frames, OIDs and Name buffers to parse the certificate
and compare its issuer. The request size depends only on the hash, the serial
number length and the nonce length. Pass an empty `TC_buffer` to query it.
A short buffer returns `TC_TLV_LIMIT` with `*length` set to the required size
and the buffer unchanged. Any other failure after the argument checks sets
`*length` to 0. The output must not overlap the inputs, the request structure,
`work` or `length`.

## Verifying a response

Set `TC_X509_ocsp_verify_request`:

- `response`: one complete DER OCSPResponse.
- `certificate` and `issuer`: the certificate and its issuer from a validated
  path.
- `expected_nonce`: the nonce sent in the request, or empty. A present nonce
  must be echoed byte for byte in responseExtensions. Leave it empty for a
  stapled or pre-produced response and rely on freshness.
- `certificates`: an optional `TC_X509_store_source` of delegate candidates,
  searched after the certs field of the response.
- `time`: a `TC_X509_revocation_time` with the evaluation time, clock skew and
  maximum age in seconds.
- `max_responses`: the most SingleResponses read. It must be nonzero.
- `max_certificates`: the most delegate candidates examined, from the response
  and the store together. Zero suffices for a response the issuer signed.
- `parsing`: limits applied to the OCSPResponse, the inner BasicOCSPResponse,
  the certificate and every delegate candidate.
- `signatures`: the signature provider.

A response is current when thisUpdate is at most `at + clock_skew_seconds`,
nextUpdate, when present, is later than `at - clock_skew_seconds`, and
producedAt is at most `at + clock_skew_seconds`. A nonzero `max_age_seconds`
also bounds the age of thisUpdate. A response without nextUpdate is current
only under a nonzero `max_age_seconds`. The same rule applies to CRLs.

The response must contain exactly one SingleResponse for the certificate. The
CertID must use SHA-1 or SHA-256 and match the issuer name hash, issuer key
hash and serial number. Other SingleResponses may cover other certificates. A
SingleResponse whose CertID uses another hash makes the whole response
`TC_TLV_UNSUPPORTED`, even when it covers another certificate. An unknown
critical extension in responseExtensions or singleExtensions is
`TC_TLV_UNSUPPORTED` (RFC 5280 section 4.2). A nonce is accepted only as a
noncritical responseExtension of 1 to 128 bytes.

## Responder authorization

RFC 6960 section 4.2.2.2 allows two signers:

- The issuer itself, named byName or byKey in the ResponderID. The signature
  must verify under `issuer->public_key`.
- A delegate that the issuer certified directly. The delegate must match the
  ResponderID and validate as a one-certificate path below the issuer at
  `time.at` with `time.clock_skew_seconds`. It needs `id-kp-OCSPSigning` in
  extendedKeyUsage and digitalSignature in a present keyUsage.
  anyExtendedKeyUsage alone is rejected. Path validation also rejects unknown
  critical extensions.

For a delegate, `responder_certificate` borrows its DER certificate from the
response or the store record, and `responder_nocheck` reports
`id-pkix-ocsp-nocheck`. Without nocheck the caller must establish the
delegate's own revocation status before relying on the result (RFC 6960
section 4.2.2.2.1). `TC_X509_path_check_revocation` does this with the CRL
index. When no CRL evidence covers the delegate, the member falls back to its
own CRL evidence, and a member without that evidence returns
`TC_TLV_UNSUPPORTED`. For an issuer-signed response `responder_certificate` is empty
and `responder_nocheck` is zero.

## Results

| Result               | Meaning                                                                                                                                                                                         |
| -------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `TC_TLV_OK`          | Authenticated and fresh. `status` is `TC_X509_REVOCATION_GOOD` or `TC_X509_REVOCATION_REVOKED`.                                                                                                 |
| `TC_TLV_UNSUPPORTED` | No decision: an authenticated unknown status, internalError, tryLater, sigRequired, unauthorized, or an unknown response type, CertID hash, version, critical extension or signature algorithm. |
| `TC_TLV_INVALID`     | Malformed DER, malformedRequest, no or a duplicate SingleResponse for the certificate, a wrong issuer, a nonce mismatch, stale or future times, removeFromCRL, or no authorized signer.         |
| `TC_TLV_LIMIT`       | Work, a parsing limit, workspace capacity, `max_responses` or `max_certificates` ran out.                                                                                                       |
| `TC_TLV_ARGUMENT`    | A required pointer is NULL, the nonce length is outside 32 to 128, `max_responses` is zero or `time.at` is invalid. A store callback or signature provider failure is also ARGUMENT.            |

REVOKED fills `revocation_time` and, when `has_reason` is set, `reason` with
the CRLReason. Read `next_update` only when `has_next_update` is set.
Argument errors found at entry leave the result and work unchanged. Every other
failure zeroes the result. UNSUPPORTED and LIMIT never report a status. Treat
them like INVALID for acceptance and fall back to CRLs or fail closed.

## Workspace and work

Both calls take a `TC_X509_path_workspace`. `frames` needs one entry per
constructed nesting level of the deepest object parsed, which is usually an
embedded delegate certificate. `oids` bounds the extensions of each extension
list. The Name buffers bound Name comparison. Delegate validation also uses one
`certificates` entry, one `summaries` entry and the policy arrays. The
workspace holds parser views only. Calls sharing it must be serialized.

`work` is a `size_t` budget. Each call charges the response and
BasicOCSPResponse bytes, the bytes hashed for each matching CertID, Name
comparison, extension walks, delegate path validation and every signature
verification. The native provider reserves
`TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK` units for each signature. A delegated
response costs roughly one path validation and two signature checks. The
example's `EXAMPLE_OCSP_WORK_LIMIT` covers every SD 33 response. Work may
change on every result.

Response, certificate, store records and issuer bytes must stay unchanged
during the call and while `responder_certificate` is used.

## Limitations

- Requests are unsigned and cover one certificate.
- CertIDs use SHA-1 or SHA-256.
- The module has no transport, cache or response pre-fetching.
- The low-level verify reports a delegate without nocheck and leaves its
  revocation check to the caller.
- `TC_X509_path_check_revocation` verifies responses without a nonce.

## Testing

`test_x509_ocsp_sd33` verifies the NIST SD 33 captured responses, checks
request encoding against OpenSSL-generated requests and runs the example.
`test_x509_ocsp_icam` covers the ICAM delegates with and without nocheck,
store-supplied delegates and the composed path check. With
`TINY_CRYPTO_TEST_OPENSSL=ON`, `test_x509_ocsp_openssl` generates responses
for issuer-signed and delegated responders and rejected delegates, CertID
hashes, duplicate SingleResponses, critical extensions, nonces, work limits
and CRL fallback. `fuzz_ocsp` fuzzes both operations. See
[Running the tests](testing.md).
