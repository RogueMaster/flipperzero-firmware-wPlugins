<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Working with the API

Include the public header for the operation you need. The C headers support
C99 and C++11. Build options determine which implementations are linked. See
the [configuration options](../README.md#configuration) and the
[test guide](testing.md), which also builds the examples. The C++11 wrappers
are described in [C++ wrappers](cpp.md).

This page holds the contracts shared by every module: results, failure and
wipe rules, input stability, work budgets, naming and argument order. Each
public header opens with a module block that links here and states its scope,
standards, configuration macros and limitations. Function comments in the
headers give the exceptions and the conditions behind each status.

## Build configuration

Each algorithm, mode, parser and protocol module is a feature with one
`config.h` macro and one CMake option. The option is `TINY_CRYPTO_` followed by
the macro name without `TC_`, so `TINY_CRYPTO_ENABLE_X509_PATH` sets
`TC_ENABLE_X509_PATH`.
[`cmake/features.json`](../cmake/features.json) is the registry. It gives each
feature its description, its parent and the sources it compiles. A switch
takes `AUTO`, `ON` or `OFF`. A value option, such as
`TINY_CRYPTO_AES_SBOX_MODE`, takes one of the names listed for it. A
sub-feature, such as `TINY_CRYPTO_AES_ENABLE_GCM`, compiles its code only
while its parent feature is on.

`config.h` owns the defaults and the dependency rules. CMake compiles it with
the selected values, so an invalid combination stops configuration with the
same `#error` text that a direct-source build reports. A `TINY_CRYPTO_*` cache
entry that names no option also stops configuration. A retired name reports
its replacement from the registry's `retired_options` map. Remove such an entry
with `cmake -U <name>` or start a new build directory.

The minimal core is `src/common.c`: secret wiping, constant-time comparison and
the span helpers shared by every module. A build with every switch `OFF`
configures, compiles and archives only that file. Shared helpers, such as the
hash core, `block_modes.c`, `mac_core.c` and the PKI storage planner, compile
while at least one feature that uses them is on. Applications that need a small
image start from the all-off build and enable features one at a time.

A public header declares a function only in configurations that compile it.
Calling a function of a disabled feature fails at compile time with an
undeclared function, before any link step. A source of a disabled feature
compiles to nothing, so a build may compile every `src/*.c` file and still
link only the enabled code.

## Result model

Every public status-returning operation uses `TC_result`. Module typedefs and value names
keep calls descriptive while allowing one handler to receive results from
different modules. Compare against the named success value and avoid treating
a result as a Boolean.

| Result name                                                                  | Returned by                                                      | Success               |
| ---------------------------------------------------------------------------- | ---------------------------------------------------------------- | --------------------- |
| `TC_status`                                                                  | AES, DES, hashes, HMAC, MD5, KMAC256, KBKDF, HKDF, SSKDF, PIV SM | `TC_OK`               |
| `TC_EC_result`, `TC_RSA_result`, `TC_GZIP_result`, `TC_key_challenge_result` | EC, RSA, GZIP, key challenges                                    | `*_OK`                |
| `TC_DRBG_result`                                                             | SP 800-90A DRBGs                                                 | `TC_DRBG_OK`          |
| `TC_TLV_result`                                                              | TLV, DER, X.509, CMS, CRL, OCSP, CVC and PIV/TWIC readers        | `TC_TLV_OK`           |
| `TC_X509_signature_result`, `TC_X509_path_status`                            | signature providers, path validation                             | `*_VALID`             |
| `TC_credential_status`                                                       | CMS, CHUID, biometric, security-object and SM validation         | `TC_CREDENTIAL_VALID` |
| `TC_TWIC_CCL_result`                                                         | TWIC canceled card lists                                         | `TC_TWIC_CCL_OK`      |
| `TC_APDU_result`                                                             | ISO/IEC 7816-4 APDU encoding and exchange                        | `TC_APDU_OK`          |
| `TC_PIV_result`                                                              | PIV and TWIC card commands                                       | `TC_PIV_OK`           |
| `TC_result`                                                                  | workspace sizing and setup helpers                               | `TC_RESULT_OK`        |

The names in the table are typedef aliases of `TC_result`. `TC_status` uses
`TC_MISMATCH`, an alias of `TC_RESULT_INVALID`, for a failed authentication or
comparison. It uses `TC_ERROR`, an alias of `TC_RESULT_ERROR`, for every other
failure, including a NULL pointer, a bad length, a short output, an overlap, a
missing IV or key, and a backend failure.

Data returned by validation and parsing uses the `_report` suffix. For
example, `TC_X509_path_report` contains a selected key and policies, while the
function that fills it returns `TC_X509_path_status`, an alias of `TC_result`.

The shared values have one meaning:

- `INVALID`: received data is malformed or fails a check. Examples are a bad
  encoding, a signature that fails verification and a point off the curve.
- `LIMIT`: a caller-supplied bound ran out. Examples are a short output
  buffer, too little workspace or frames, an exhausted work budget, too few
  random attempts and a `TC_TLV_limits` bound. More resources may succeed.
- `ARGUMENT`: a caller error. Examples are a NULL pointer, a span with NULL
  data and a nonzero length, a forbidden overlap and a parameter outside the
  function's domain.
- `UNSUPPORTED`: well-formed input that uses an algorithm, size, version or
  feature outside this library or build.
- `ERROR`: a random source, signature provider, cipher backend, storage
  source or internal self-check failed.

RSA adds `TC_RSA_IN_PROGRESS` and `TC_RSA_CANCELLED` for stepwise key
generation. PIV card commands add `TC_PIV_CARD_STATUS`, a card answer other
than success, and `TC_PIV_REFUSED`, a safety rule that stopped a
command before it was sent. `TC_DRBG_ENTROPY` reports a failed entropy source with the DRBG
state unchanged. `TC_TLV_END` and `TC_TLV_MORE` are reader states: no more
siblings, or a root reader that needs more input. `TC_TLV_IO` reports backing
storage that failed to supply bytes, and path and credential validation
report it as their ERROR value. `TC_X509_REVOCATION_UNDETERMINED` means
no evidence covers a certificate. `TC_CREDENTIAL_REVOKED` and
`TC_CREDENTIAL_UNAVAILABLE` distinguish revoked credentials from missing trust
or evidence.

UNSUPPORTED and LIMIT never report success. A validation that cannot finish
because of an unsupported feature or an exhausted limit fails closed.

## Failure state and wiping

These rules hold for every public function unless its header states an
exception:

1. Each public entry checks its arguments once, before any write. An argument
   error leaves every output, context, workspace, work budget and random
   source unchanged. So does a capacity or work preflight that returns LIMIT
   before processing starts.
1. Readers and parsers write their result only on success. A failed read
   leaves `out` unchanged.
1. After the argument checks, a failure wipes any output that could hold
   partial plaintext, keys or unauthenticated data. The output then holds
   zero bytes. In-place callers lose the input.
1. Frames, workspaces and other scratch are provisional. Any failure may
   change them. Scratch that held secrets is wiped before return.
1. Work budgets decrease by the work completed, on success and on failure.
1. A final or finish call consumes its context and wipes it. A failure while
   processing clears the context, so no partial chaining value or key stays
   usable. `*_clear` functions always wipe and accept a NULL context.
1. Key schedules, MAC states and stack secrets are wiped before return.

Wiping is unconditional, and defining `TC_ZEROIZE` or `TC_STRICT` stops the
build with an `#error`. `TC_secure_zero` is a best-effort wipe for
application buffers. Copies held in CPU registers remain. Clear
application-held keys and plaintext with `TC_secure_zero` when their lifetime
ends, and release acquired snapshots on every exit path.

### Documented exceptions

These functions depart from the rules above by design. Their headers give
the details.

- Modules that return `TC_status` (AES, DES, hashes, HMAC, MD5, KMAC256,
  KBKDF, HKDF, SSKDF and PIV SM) report a short output buffer as `TC_ERROR`,
  because `TC_status` has no LIMIT value.
- `TC_TWIC_CCL_stream_update` makes argument errors sticky after init. The
  stream returns the first error until the next init, so a rejected chunk
  can never be skipped.
- `TC_CMS_signer_verify_digest` checks the digest length after algorithm
  resolution, because the resolved hash sets the length. An argument error
  there can follow charged work.

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
written. Spare capacity follows the operation's documented contract.
For key derivation, see the [HKDF guide](hkdf.md) for PRK lifetime, output
limits, and hybrid shared-secret inputs.

`TC_buffer` pairs writable `data` with byte `capacity`. `TC_random_source` pairs
a random-fill callback with its context. Random callbacks must fill the entire
requested buffer before returning `TC_OK`.

### Input stability and overlap

- Keep every input stable for the duration of the call. No other thread,
  interrupt handler or DMA transfer may write it. Some operations read an
  input twice. GCM, CCM, EAX and EAX' decryption authenticate the ciphertext
  and then decrypt it.
- Keep parsed input stable while any borrowed view of it is in use.
- Inputs may share storage with each other.
- Outputs, contexts, workspaces and work counters must be disjoint from the
  inputs and from each other unless the header permits in-place use. The
  block-mode, AEAD, key wrap, KMAC and MD5 functions list their permitted
  in-place and overlap cases.
- Checked overlaps return the module's argument error before any write. An
  overlap that a function cannot detect, such as one through a separate
  mapping of the same memory, is undefined behavior.

## Workspaces and limits

Workspaces hold caller-owned scratch. Use static or application-owned storage
for large RSA and certificate-validation workspaces on constrained devices.
Keep workspace metadata alive for every operation that refers to it.
Descriptors that only point at caller storage, such as `TC_X509_workspace`,
`TC_X509_path_workspace`, `TC_CMS_path_workspace` and `TC_RSA_workspace`, are
passed as const pointers or as `TC_TLV_frames` values, and calls write the
storage they point to. Storage types such as `TC_EC_workspace` and
`TC_GZIP_workspace`, and readers, contexts and results, are passed mutable.

The [validation setup guide](validation.md) shows checked arena sizing and the
micro, mini, and desktop capacity presets. RSA calls group scheme settings in
operation options and random/work limits in an execution descriptor.

Calls sharing writable scratch require application serialization. Independent
contexts and disjoint scratch can be used concurrently, subject to provider and
storage callback requirements.

Parsing limits bound input size, value size, element count and nesting depth.
A limit result requires an explicit application decision about retrying with
additional resources.

## Work budgets

Operations that can run long take an explicit work budget. Two types exist,
with different units.

`TC_work_budget` holds a `uint32_t remaining` count of algorithm units. EC,
RSA and key challenges take it, usually inside an execution descriptor.

- EC: one unit per curve bit for each scalar multiplication or modular
  inversion, and one unit per point validation and random request.
  `TC_EC_operation_work(curve, operation)` returns the exact cost of one
  operation or one attempt of a randomized operation.
- RSA: modular operations and encoding comparisons. `TC_RSA_public_work`,
  `TC_RSA_private_work`, `TC_RSA_encode_v15_work`, `TC_RSA_encode_pss_work`,
  `TC_RSA_oaep_work`, `TC_RSA_VALIDATE_WORK` and `TC_RSA_KEYGEN_STEP_WORK`
  give the exact or sufficient costs. See [RSA work budgets](rsa.md#work-budgets).
- These operations check their full cost before arithmetic or any random
  request. A short budget returns LIMIT with outputs, work and the random
  source unchanged.

`size_t* work` is a remaining count of processing units for parsing and
validation. X.509, CMS, CRL, OCSP, path validation, credential validation,
LDS, PIV card identifiers, PIV CVC chains, SM authentication and GZIP take it.

- A unit is one byte examined or one element, candidate, comparison or
  decoding step. Each header names what its function charges, such as
  traversed bytes, digest input or table entries.
- Charges are incremental. A charge larger than the remaining budget returns
  the module's LIMIT value, and no later step runs. The PKI modules then set
  `*work` to zero. GZIP leaves the unspent remainder in `*work`.
- Signature verification inside these APIs first charges one unit plus the
  signed bytes, signature and key encoding, then the provider consumes its own
  work and never increases it. The native provider reserves
  `TC_X509_native_workspace.signature_work` for each attempt, and
  `TC_X509_NATIVE_DEFAULT_SIGNATURE_WORK` covers the supported curves and RSA
  sizes with ordinary exponents.
- Share one counter across related calls, such as the init and next calls of
  a reader or the steps of one credential decision, so the whole operation has
  one bound.

Size a `size_t` budget from the input size and the number of passes. Start
with a few times the total input bytes plus one signature reservation per
signature the operation can check, then tune it against the largest inputs the
application accepts. Work units are independent of elapsed time.

## Naming and argument order

Public C names start with `TC_` and a module prefix, such as `TC_AES_`,
`TC_X509_` or `TC_PIV_SM_`. Types use a lowercase noun after the prefix, such
as `TC_X509_certificate` and `TC_EC_workspace`. Enumerators and macros are
uppercase. Configuration macros are `TC_ENABLE_*` for a module and
`TC_<MODULE>_ENABLE_*` for a mode or variant. Each CMake option is
`TINY_CRYPTO_` followed by its macro name without `TC_`. C++ wrappers use
lowercase functions and classes named for the algorithm in namespace
`tiny_crypto`.

Function names end with the operation:

- `init`, `update`, `final` or `finish`, and `ctx_clear` or `clear` for
  stateful contexts. `set_iv` loads an IV for the next message.
- `read` parses one object into borrowed views. `next` returns the next item
  from a reader, and `END` reports the end.
- `write` and `encode` produce an encoding into a caller buffer.
- `verify` checks a signature, MAC or tag. `validate` applies a policy or a
  complete path, key or credential check. `check` tests one property or one
  kind of evidence, such as revocation.
- `_work`, `_size`, `_BYTES` and `_WORDS` return the cost or storage a call
  needs, so callers can size budgets and buffers before the call.
- `_short_tag` selects tag lengths below the default minimum.
- `wrap` and `unwrap` protect and recover key data under a key-encryption key.

Arguments follow the operation:

1. The context, session or reader, when the call has one.
1. Configuration: algorithm, profile, options or policy.
1. Inputs, as `TC_bytes` spans or input structures.
1. For parsers and validators: limits, frames, workspace and the `size_t`
   work counter, then the output last.
1. For cryptographic one-shots: outputs after the inputs. EC and RSA then take
   the workspace and the `TC_work_budget` or execution descriptor last.

A call that needs more than eight parameters takes a `*_request` structure
and a context, such as `TC_CMS_validation_request` with a
`TC_validation_context`. No public function takes more than eight parameters.

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

See [Revocation](#revocation) for CRL and OCSP evidence.

PIV/TWIC object policy and the final access decision require their own checks.
Select compatibility options explicitly, including TWIC signed/unsigned CHUID
profiles and CMS signed-attribute encoding.

`TC_PIV_printed_read` parses PIV printed information or decrypted TWIC DFC109
contents. Select the profile and whether the input includes the outer `53`
container. The returned text fields borrow the input buffer. PIV dates use
`YYYYMMMDD`. TWIC dates use `DDMMMYYYY`.

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
Iris credential validation reports unsupported because the library has no
ISO/IEC 19794-6 record reader.

After authenticating the Security Object and signed CHUID, call
`TC_PIV_printed_expiration_check`. It requires the printed date to match the
CHUID date and checks the application evaluation time through the end of that
day. A successful parse supplies structure only. Authentication and freshness
come from the validation calls.

The [credential validation example](credential-validation.md) composes certificate trust,
identifier checks, cancellation, fresh key possession, signed objects and a
bounded list of biometric objects using retained inputs. Reader commands live in the example
application's proof callback and transport layer.

`TC_PIV_CHUID_read` also provides `TC_CHUID_PROFILE_LEGACY_KEY_MAP`
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
`TC_CMS_credential_workspace`. A successful `TC_PIV_CHUID_report` supplies the
authenticated object and signer to dependent biometric and Security Object
requests, which reject a result from another profile or evaluation time.
Callers retain responsibility for transport, cancellation status, and
authorization.

## Results and cleanup

Use the named results for the operation you called. See the
[result model](#result-model). Setup helpers return `TC_result`, and successful
setup returns `TC_RESULT_OK`.

Handle malformed input, unsupported algorithms, exhausted limits and unavailable
evidence explicitly. A successful parse supplies structure for later checks.
CMS and CVC credential workflows share `TC_credential_status`, including distinct
revoked, unavailable, unsupported, limit and error outcomes.
Accept a credential only after every required authentication and policy check
has succeeded.

Public declarations specify which outputs survive failure and which scratch or
work counters may change, following the
[failure and wipe rules](#failure-state-and-wiping).

## Revocation

CRL and OCSP evidence share `TC_X509_revocation_status` and the freshness rule
of `TC_X509_revocation_time`. `TC_X509_path_check_revocation` checks a
validated path, anchor-issued certificate first and target last. See
[X.509 path revocation](x509-revocation.md) for configuration and results.

`TC_validation_options.revocation` selects the revocation evidence policy of
the CMS, X.509 and credential validators (`TC_validation_revocation`). The zero
value, `TC_VALIDATION_REVOCATION_REQUIRED`, returns `TC_CREDENTIAL_UNAVAILABLE`
when a path member has no current CRL evidence.
`TC_VALIDATION_REVOCATION_WHEN_AVAILABLE` accepts such a member and reports
`revocation_checked` 0 in the result. Covering CRLs are still checked under
both values, so a revoked member returns `TC_CREDENTIAL_REVOKED`, and an
unsupported CRL returns `TC_CREDENTIAL_UNSUPPORTED`. Select the policy at the
application boundary and treat `revocation_checked` 0 as missing evidence.

`<tiny_crypto/x509_ocsp.h>` encodes bounded OCSP requests and verifies complete
DER OCSP responses, including stapled responses. Supply the certificate and its
issuer from a validated path, a signature provider, a `TC_X509_revocation_time`
and a `TC_X509_path_workspace`. The BasicOCSPResponse inside the response is
parsed under the same `TC_TLV_limits` as the outer response. A delegated
responder is validated as a one-certificate path below the issuer with the
OCSPSigning purpose. `responder_certificate` then borrows its certificate and
`responder_nocheck` reports `id-pkix-ocsp-nocheck`. Without nocheck, the caller
establishes the delegate's revocation status (RFC 6960 4.2.2.2.1).
`TC_X509_ocsp_request_encode` reports the required size with `TC_TLV_LIMIT`
for a short or empty buffer. OCSP requires SHA-1.

`TC_X509_ocsp_response_verify` returns `TC_TLV_OK` only for an authenticated,
fresh GOOD or REVOKED status, with the CRLReason when the response has one. An
authenticated unknown status and the unsigned error responses (internalError,
tryLater, sigRequired, unauthorized) return `TC_TLV_UNSUPPORTED`. CRL and OCSP
results share `TC_X509_revocation_status` and the freshness rule of
`TC_X509_revocation_time`. `TC_X509_path_check_revocation` accepts one OCSP
response per path member and falls back to CRLs for a member without an
accepted response. It accepts a delegate without nocheck only when the CRL
index proves that delegate unrevoked. The CMS and credential validation APIs
use CRL evidence only. See [X.509 OCSP](x509-ocsp.md) for request encoding,
responder authorization, workspace sizing and the example.

## Authenticated encryption

The one-shot AEAD functions in `<tiny_crypto/aes.h>` (GCM, CCM, EAX, EAX' and
SIV) share one contract.

- Encrypt writes `plaintext.length` bytes of ciphertext and `tag.capacity` tag
  bytes. Decrypt writes `ciphertext.length` bytes of plaintext and checks
  `tag.length` tag bytes. EAX' and SIV use fixed-size tag arrays.
- The text output capacity must be at least the text input length. Bytes past
  the text length are never written.
- Text input and output are the same buffer or fully disjoint. The tag is
  disjoint from the text output. SIV associated data is disjoint from the text
  output. A violation returns `TC_ERROR` before a write.
- The key, and the nonce and associated data of GCM, CCM, EAX and EAX', may
  share storage with the text output. Each is read in full before the first
  output write.
- The caller keeps the key, nonce, associated data, text input and received
  tag stable for the duration of the call. No other thread or DMA transfer may
  write them. GCM, CCM, EAX and EAX' decrypt read the ciphertext twice, once to
  authenticate it and once to decrypt it.
- `TC_ERROR` before any write reports a NULL key or tag, a NULL span with a
  nonzero length, a short text output, a forbidden overlap, or a nonce, tag or
  text length outside the mode's range.
- GCM, CCM, EAX and EAX' verify the tag before writing plaintext. SIV decrypt
  writes candidate plaintext first and recomputes the synthetic IV over the
  associated data and that plaintext (RFC 5297 section 2.7).
- After the argument checks, every failure wipes the text output. A tag that
  fails to verify returns `TC_MISMATCH`. A cipher backend failure returns
  `TC_ERROR`. In-place callers lose the input in both cases. The tag output is
  written only on success.

GCM decryption is one-shot. The streaming `TC_AES_GCM_ctx` API encrypts only.
Use `TC_AES_GCM_encrypt_short_tag` and `TC_AES_GCM_decrypt_short_tag` for the
4 and 8-byte tags of SP 800-38D appendix C.

```c
TC_status status = TC_AES_GCM_decrypt(key, (TC_bytes){iv, 12}, (TC_bytes){aad, aad_length},
                                      (TC_bytes){packet, packet_length},
                                      (TC_bytes){tag, 16},
                                      (TC_buffer){packet, packet_length});
if (status != TC_OK)
  return status; /* packet holds no plaintext */
```

## Key wrap

`<tiny_crypto/aes_kw.h>` provides one-shot SP 800-38F KW and KWP wrap and
unwrap. Unwrap uses the output as its working area and checks the integrity
value after the inverse wrapping function. Every failure after the argument
checks wipes that area. A failed integrity, length indicator or padding check
returns `TC_MISMATCH`. Inputs and outputs may overlap in any way. See
[AES key wrap](aes-kw.md) for KEK lengths, buffer sizes and limits.

## Tag lengths

`TC_MIN_TAG_LEN` in `config.h` sets one minimum tag length for CCM, EAX,
AES-CMAC, DES-CMAC, HMAC and KMAC. It defaults to 8 bytes, the 64-bit floor of SP 800-38B
Appendix A.2, and a build may raise it to 16. Values outside 8..16 stop the
build. A value above 8 also requires `TC_DES_ENABLE_CMAC=0`, since a DES-CMAC
tag holds at most 8 bytes.

| Mode       | Default entry points                                      | `_short_tag` entry points                      |
| ---------- | --------------------------------------------------------- | ---------------------------------------------- |
| CCM        | even lengths from `TC_MIN_TAG_LEN` to 16                  | even lengths from 4 up to `TC_MIN_TAG_LEN - 1` |
| EAX        | `TC_MIN_TAG_LEN`..16                                      | 1..`TC_MIN_TAG_LEN - 1`                        |
| AES-CMAC   | `TC_MIN_TAG_LEN`..16                                      | 1..`TC_MIN_TAG_LEN - 1`                        |
| DES-CMAC   | `TC_MIN_TAG_LEN`..8                                       | 1..`TC_MIN_TAG_LEN - 1`                        |
| GCM        | 12..16                                                    | 4 or 8 (SP 800-38D appendix C)                 |
| ISO 9797-1 | 8                                                         | 4..7                                           |
| HMAC       | `max(TC_HMAC_MIN_TAG_LEN, TC_MIN_TAG_LEN)`..digest length | 1..default minimum - 1                         |
| KMAC256    | `TC_MIN_TAG_LEN`..`UINT64_MAX / 8`                        | 1..`TC_MIN_TAG_LEN - 1`                        |

Each accepted length has exactly one entry point. The other entry point returns
`TC_ERROR` and leaves every output unchanged. A zero-length tag is never
accepted. Short tags are the leading bytes of the full tag. Call a
`_short_tag` form only when the protocol fixes that tag length and limits the
number of failed verifications for a key. EAX' is exempt from the minimum
because ANSI C12.22 fixes its tag at 4 bytes.

```c
/* A protocol with 4-byte CCM tags selects the short-tag call. */
TC_status status = TC_AES_CCM_decrypt_short_tag(key, (TC_bytes){nonce, 13},
                                                (TC_bytes){header, header_length},
                                                (TC_bytes){packet, packet_length},
                                                (TC_bytes){tag, 4},
                                                (TC_buffer){packet, packet_length});
if (status != TC_OK)
  return status; /* packet holds no plaintext */
```

## C++ wrappers

The C++11 wrappers in namespace `tiny_crypto` return the C result types, take
`bytes` and `buffer` spans, and own and wipe their contexts. See
[C++ wrappers](cpp.md) for conventions, lifecycles and examples.

## Block cipher modes

Key a context with `TC_AES_init` or `TC_DES_init`. Init leaves the context
without an IV. Load a fresh IV with `TC_AES_set_iv` or `TC_DES_set_iv` before
each message in an IV mode (CBC, CTR, OFB and the DES CFB modes). Until then
those modes return `TC_ERROR`, even for an empty buffer, so a missing
`set_iv` call cannot encrypt under a fixed zero IV. ECB and the MACs need no
IV. Calls after `set_iv` continue one message: CBC and CFB chain from the last
ciphertext, and CTR and OFB continue the keystream. The IV stays loaded until
the next init or clear, so start every new message with `set_iv`. SP 800-38A
section 5.3 and Appendix C require an unpredictable CBC and CFB IV and a
unique OFB IV per message, and Appendix B unique CTR counter blocks under one
key. Clear the context with `TC_AES_ctx_clear` or `TC_DES_ctx_clear` when its
lifetime ends.

The AES and DES CBC, CTR, OFB and ECB functions, and the DES CFB functions,
transform `buf` in place. They share one argument rule:

- The context must be initialized, and an IV mode needs an IV from `set_iv`.
  `buf` may be `NULL` only with length zero.
- CBC lengths are a multiple of the block size.
- `buf`, an IV passed to `set_iv`, and CMAC or ISO 9797 input and tags must be
  disjoint from the context. A span inside the context would change round
  keys, the feedback register or the MAC state during the call.

An argument error returns `TC_ERROR` and leaves the buffer and context
unchanged. A block cipher failure part way through a call wipes the buffer and
clears the context, so neither partial output nor a broken chaining value
survives. ECB and dynamic-key CBC take a const key, so their failures wipe the
buffer and, for CBC, the IV. CTR returns `TC_ERROR` without output when a
request needs a counter block beyond the space of the IV.

## DES and TDEA

Enable `TINY_CRYPTO_ENABLE_DES=ON` and include `<tiny_crypto/des.h>`. One
`struct TC_DES_ctx` serves single DES and TDEA. The key length passed to
`TC_DES_init` selects the cipher. `TC_DES_KEYLEN` (8 bytes) selects single
DES. `TC_DES_KEYLEN_2KEY` (16) and `TC_DES_KEYLEN_3KEY` (24) select two- and
three-key TDEA when `TINY_CRYPTO_DES_ENABLE_TDES` is on. Every mode function takes the
same context. A failed init wipes the context, so an earlier key cannot be used
after a failed re-init. `TC_DES_set_iv` loads the IV for the first message and
starts each later message under the same key.

CFB64 processes whole 8-byte segments. A call may end with one short segment,
which finishes the message. Later CFB64 calls return `TC_ERROR` until a new IV
is set. Split a stream at multiples of 8 bytes to get the same output as a
single call.

```c
#include <tiny_crypto/des.h>

int main(void)
{
    static const uint8_t key[TC_DES_KEYLEN_3KEY] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01,
        0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23
    };
    static const uint8_t iv[TC_DES_BLOCKLEN] = {0};
    uint8_t data[16] = "legacy payload";
    struct TC_DES_ctx ctx;
    int failed;

    failed = TC_DES_init(&ctx, (TC_bytes){key, sizeof key}) != TC_OK ||
             TC_DES_set_iv(&ctx, (TC_bytes){iv, sizeof iv}) != TC_OK ||
             TC_DES_CTR_crypt(&ctx, (TC_buffer){data, sizeof data}) != TC_OK;
    TC_DES_ctx_clear(&ctx);
    return failed;
}
```

## DES message authentication

Enable both `TINY_CRYPTO_ENABLE_DES=ON` and `TINY_CRYPTO_DES_ENABLE_ISO9797=ON` to use
ISO/IEC 9797-1 MAC algorithms 1 and 3. Include `<tiny_crypto/des.h>`.
`TC_DES_ISO9797_ALG1` accepts 16 or 24-byte TDEA keys. `TC_DES_ISO9797_ALG3`,
the retail MAC, accepts exactly 16 bytes, K1 || K2, and finishes with D(K2)
then E(K1). `TC_DES_ISO9797_ALG3_3KEY_EXTENSION` takes exactly 24 bytes and
finishes with E(K3). That extension is outside ISO/IEC 9797-1.
ISO/IEC 9797-1:2011 clause 5 restricts single DES to Algorithms 3 and 4.
Every build rejects a key with K1 = K2 or K2 = K3, compared without parity
bits, since such a key cancels a DES stage and clause 7.4 requires independent
keys. `TINY_CRYPTO_DES_REJECT_WEAK_KEYS=ON` also rejects weak and semi-weak
component keys.
Choose no padding for block-aligned input, method 1
for zero padding of a partial block, or method 2 for an `0x80` byte followed by
zeroes. Method 1 processes an empty message as one zero block. No padding
requires a nonempty message. The protocol must
fix or authenticate the message length when using either of those choices.

`TC_DES_ISO9797_MAC` and `TC_DES_ISO9797_verify` require the full 8-byte MAC.
Use the explicit `_short_tag` forms for 4 to 7 leading bytes when a protocol
requires truncation. Verification returns `TC_MISMATCH` for a different tag.
For incremental input, call `TC_DES_ISO9797_init`, `update`, and `final` in
order. Successful finalization consumes and clears the context. An update with
invalid arguments leaves the context unchanged, and an update that fails while
processing clears it. Input and tag buffers that overlap the context return
`TC_ERROR` with the context unchanged. A key that overlaps it returns
`TC_ERROR` from init.

```c
#include <tiny_crypto/des.h>

int main(void)
{
    const uint8_t key[16] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    const uint8_t message[] = "Now is the time for all ";
    uint8_t tag[TC_DES_BLOCKLEN];

    if (TC_DES_ISO9797_MAC(TC_DES_ISO9797_ALG3, TC_DES_ISO9797_PAD2,
                           (TC_bytes){key, sizeof key},
                           (TC_bytes){message, sizeof message - 1},
                           (TC_buffer){tag, sizeof tag}) != TC_OK)
        return 1;

    /* Use tag with the message in the application protocol. */
    TC_secure_zero(tag, sizeof tag);
    return 0;
}
```

## Complete workflows

- [Validation setup](validation.md) covers arenas, shared contexts, and
  borrowed results.
- [CMS validation](cms.md) covers content, signer paths, CRLs and a runnable tool.
- [Certificate paths](x509-path.md) explains trust inputs and path construction.
- [PIV CVC validation](piv-cvc.md) covers signer trust and secure-messaging
  CVC chains.
- [Trust stores](x509-store.md) covers snapshot ownership and publication.
- [Trust anchors](x509-trust-anchors.md) covers RFC 5914 input and RFC 5937
  path constraints.
- [TWIC cancellation lists](twic-ccl.md) covers import, lookup and freshness.
- [Credential reader](credential-reader.md) describes the supported card checks.
- [GZIP decoding](gzip.md) shows bounded output and caller-owned scratch.
- [TLV parsing](tlv.md) and [DER values](der.md) cover bounded readers.
- [AES key wrap](aes-kw.md) covers KEK lengths, buffer sizing and unwrap
  failure.
- [HKDF](hkdf.md), [DRBG](drbg.md), [RSA](rsa.md) and
  [elliptic curves](ec.md) cover the cryptographic modules.
- [C++ wrappers](cpp.md) covers the C++11 API.

Run the [documented test suites](testing.md) for the features your build enables.
