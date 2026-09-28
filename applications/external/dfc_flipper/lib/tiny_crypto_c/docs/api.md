<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Working with the API

Include the public header for the operation you need. The C headers support
C99 and C++11; C++ wrappers live in `tiny_crypto`. Build options determine which
implementations are linked. See the [configuration options](../README.md#configuration)
and [installed examples](testing.md).

## Buffers and lifetimes

`TC_bytes` describes borrowed, immutable bytes:

```c
TC_bytes message = {encoded, encoded_length};
```

The structure owns no storage. Keep `encoded` alive and unchanged while a
reader, parsed object, or validation result refers to it. An empty span may use
`NULL` with length zero. Individual schemas can require nonempty input.

Parsers return views into the input. Reusing a receive buffer invalidates those
views. Finish operations that consume the views before receiving another object,
or copy the specific bytes your application needs to retain.

Output arrays use explicit capacities. A returned length describes the bytes
written; spare capacity remains governed by the operation's documented contract.
Check each declaration for permitted in-place use and overlap restrictions.

`TC_buffer` pairs writable `data` with byte `capacity`. `TC_random_source` pairs
a random-fill callback with its context. Random callbacks must fill the entire
requested buffer before returning `TC_OK`. `TC_work_budget.remaining` is a
shared 32-bit operation allowance. Calls reduce it by work completed on success
and failure.

## Workspaces and limits

Workspaces hold caller-owned scratch. Use static or application-owned storage
for large RSA and certificate-validation workspaces on constrained devices.
Keep workspace metadata alive for every operation that refers to it.

The [validation setup guide](validation.md) shows checked arena sizing and the
micro, mini, and desktop capacity presets. RSA calls group scheme settings in
operation options and random/work limits in an execution descriptor.

Calls sharing writable scratch require application serialization. Independent
contexts and disjoint scratch can be used concurrently, subject to provider and
storage callback requirements.

Parsing limits bound input size, value size, element count and nesting depth.
Where an operation takes a work counter, initialize it before the operation and
reuse it across the related calls. A limit result requires an explicit application
decision about retrying with additional resources. Work units follow the API's
accounting rules; they do not measure elapsed time.

## Parsing, signatures and trust

Choose the operation that answers your application's question:

- Readers check the documented encoding and schema and return borrowed views.
- Signature verification authenticates bytes using a supplied key.
- Path validation checks a certificate against supplied trust and policy.
- CMS credential validation combines content binding, signature verification,
  path construction and CRL revocation checking for one selected signer.

CMS parsing and key-based signature verification use `<tiny_crypto/cms.h>`.
Include `<tiny_crypto/cms_validation.h>` only when signer discovery, paths, or
revocation are part of the application. `<tiny_crypto/validation.h>` adds the
shared arena and policy context for complete CMS and X.509 validation. Both
validation headers are selected by `TINY_CRYPTO_ENABLE_CMS_VALIDATION`.

PIV/TWIC object policy and the final access decision require their own checks.
Select compatibility options explicitly, including TWIC signed/unsigned CHUID
profiles and CMS signed-attribute encoding.

`TC_PIV_printed_read` parses PIV printed information or decrypted TWIC DFC109
contents. Select the profile and whether the input includes the outer `53`
container. The returned text fields borrow the input buffer. PIV dates use
`YYYYMMMDD`; TWIC dates use `DDMMMYYYY`.

`TC_PIV_fingerprint_read` validates an INCITS 378-2004 minutiae record against
the PIV card profile. It checks the fixed header, two finger views, minutiae,
dimensions, resolution, quality values, and the required empty extension areas.
The accepted record remains in caller-owned storage for a biometric matcher.

`TC_PIV_face_read` validates INCITS 385-2004 record framing and each image block.
`TC_PIV_FACE_PROFILE_PIV` applies the SP 800-76-2 Full Frontal and minimum-width
requirements. `TC_PIV_FACE_PROFILE_TWIC` accepts the Basic records found on
TWIC credentials while retaining the structural, image-encoding, color-space,
source, pose, feature-point, and length checks. Use
`TC_PIV_face_image_read` to obtain a borrowed image and its dimensions.

`TC_PIV_biometric_validation_request.format` binds an authenticated CBEFF object
to its expected modality and record type. Set `require_current` when the CBEFF
validity period is part of the credential decision. The comparison uses the
shared validation-context time, including both boundary instants.
Iris credential validation requires an ISO/IEC 19794-6 record reader and
currently reports unsupported.

After authenticating the Security Object and signed CHUID, call
`TC_PIV_printed_expiration_check`. It requires the printed date to match the
CHUID date and checks the application evaluation time through the end of that
day. A successful parse alone carries no authentication or freshness decision.

The [credential validation example](credential-validation.md) composes certificate trust,
identifier checks, cancellation, fresh key possession, signed objects and a
bounded list of biometric objects using retained inputs. Reader commands live in the example
application's proof callback and transport layer.

`TC_PIV_CHUID_read_profile` also provides `TC_CHUID_PROFILE_LEGACY_KEY_MAP`
for PIV-shaped CHUIDs containing the historical Authentication Key Map (`3D`).
Select this profile explicitly for compatible credentials. It accepts one map
of up to 512 bytes immediately before the signature and returns its borrowed
value in `authentication_key_map`. `signed_content` includes the map's exact
tag, length and value. Authenticate the signature before using the map.
An empty present map has a non-NULL pointer.
Current PIV and TWIC profiles retain their field schemas. See
[SP 800-73-2, Part 1, Table 8](https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-73-2.pdf)
for the field definition.

`TC_PIV_CHUID_validate` is the public signed-CHUID composition workflow. Its
request binds borrowed CHUID bytes to identifiers and expiration from an already
validated card certificate, with an explicit PIV, TWIC Legacy, or TWIC NEXGEN
profile. `TC_validation_options` provides one evaluation time and signature
provider plus separate certificate and CRL-signer policies. Initialize a
`TC_validation_context` with held certificate/CRL trust and a sized
`TC_CMS_credential_workspace`. A successful `TC_PIV_CHUID_result` supplies the
authenticated object and signer to dependent checks. Callers retain responsibility
for transport, cancellation status, and authorization.

## Results and cleanup

Use the named results for the operation you called. `TC_status`, parser results,
signature results and credential verdicts have distinct contracts. Compare
against the exact success or acceptance enumerator; avoid treating a result as
a Boolean or converting between enums numerically.

Setup helpers return `TC_result`; successful setup is `TC_RESULT_OK`.

Handle malformed input, unsupported algorithms, exhausted limits and unavailable
evidence explicitly. A successful parse supplies structure for later checks.
CMS and CVC credential workflows share `TC_credential_status`, including distinct
revoked, unavailable, unsupported, limit and error outcomes.
Accept a credential only after every required authentication and policy check
has succeeded.

Public declarations specify which outputs survive failure and which scratch or
work counters may change. Secret-producing operations also specify wiping
behavior. Clear application-held keys and plaintext with `TC_secure_zero` when
their lifetime ends. Release acquired snapshots on every exit path.

## Complete workflows

- [Validation setup](validation.md) covers arenas, shared contexts, and
  borrowed results.
- [CMS validation](cms.md) covers content, signer paths, CRLs and a runnable tool.
- [Certificate paths](x509-path.md) explains trust inputs and path construction.
- [PIV CVC validation](piv-cvc.md) covers signer trust and secure-messaging
  CVC chains.
- [Trust stores](x509-store.md) covers snapshot ownership and publication.
- [TWIC cancellation lists](twic-ccl.md) covers import, lookup and freshness.
- [Credential reader](credential-reader.md) describes the supported card checks.
- [GZIP decoding](gzip.md) shows bounded output and caller-owned scratch.

Run the [documented test suites](testing.md) for the features your build enables.
