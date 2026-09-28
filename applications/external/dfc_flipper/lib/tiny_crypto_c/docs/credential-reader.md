<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Credential reader example

`credential_check` uses PC/SC to select the TWIC and PIV applications and read
their CHUID and card-authentication certificate containers. It checks application
identity, CHUID fields, certificate-container fields and X.509 certificate structure.
GZIP certificates are decoded with bounded output and work. Signature and trust
verification are separate;
the command's exit status must never be used as a credential acceptance decision.

Build against an installed tiny_crypto_c package. The `desktop` resource profile
enables the algorithms and parsers used by both commands, including legacy SHA-1.
For a custom profile, enable TLV, DER, PIV CHUID, X.509, GZIP, TWIC CCL,
RSA and EC,
plus the hashes needed by the certificates:

```sh
cmake -S examples/credential_check -B build/credential-check \
  -DCMAKE_PREFIX_PATH=/path/to/tiny-crypto-install
cmake --build build/credential-check
ctest --test-dir build/credential-check --output-on-failure
build/credential-check/credential_check --reader 'Your PC/SC reader name'
```

## Authenticate a TWIC card

For an APDU-independent view of the validation policy, see
[Composing PIV and TWIC validation](credential-validation.md). That example accepts retained
objects and trust snapshots and delegates fresh key possession to an
application callback.

The same build produces `twic_authenticate`, which performs the active-card
authentication workflow using locally provisioned trust material:

```sh
build/credential-check/twic_authenticate \
  --reader 'Your PC/SC reader name' \
  --root /path/to/approved-root.der \
  --root-sha256 64-hex-digit-provisioned-fingerprint \
  --issuer /path/to/issuer.der \
  --ccl /path/to/provisioned-ccl.bin \
  --marsec-level 1
```

`--marsec-level` accepts 1, 2 or 3 and defaults to 1. CCL files may be up to
seven days old at level 1, or one day old at levels 2 and 3. The command measures
age from the file's creation time. It rejects a file if that time is unavailable
or in the future.

`--root-sha256` pins the exact DER root. Obtain the fingerprint through the same
trusted provisioning channel as the root approval. A changed root file fails
before card access.

Provision the latest obtained list. Copying or rebuilding a file can reset its
creation time; preserve the acquisition date when preparing the packed image.
The operator must also refresh the list within 12 hours of a MARSEC increase.
Track level changes in the system that supplies the CCL. See
[33 CFR 101.525](https://www.law.cornell.edu/cfr/text/33/101.525).

Add `--extended-reads` when the reader and card support extended APDUs. This
requests each object within the supplied buffer limit and follows `61xx` with
GET RESPONSE. A `6282` end-of-object warning is accepted only with a complete
`53` envelope. Truncated objects fail even when the card returns `9000`.

Use `--piv-certificate-envelope` when the TWIC application's card-authentication
certificate uses the PIV `70`, `71`, empty `FE` envelope. The default TWIC
envelope contains `70` and `71`. This option selects container framing;
certificate, signature and trust checks remain in effect.

`--rsa-padding pss` selects an RSA-PSS card challenge with SHA-256, MGF1-SHA-256,
and a fresh 32-byte salt. The default is `v15` (PKCS #1 v1.5 with SHA-256).
This option controls the card challenge. Each certificate signature is checked
with the algorithm recorded in that certificate. EC challenges use SHA-256 for
P-256 and SHA-384 for P-384. The command stops if the card proof fails.

Use `--printed-plaintext` when the TWIC Security Object contains a hash of the
decrypted printed-information TLVs. Also supply `--security-object` and the
privacy key through `--tpk-hex`. The command keeps the decrypted
bytes for the final hash check, parses the DFC109 fields, and checks their date
against the signed CHUID and current validation time. It wipes the plaintext
during cleanup.

By default, the command hashes the stored object. Choose the hash input required
by the card profile. The command rejects a hash mismatch.

The optional `--minimum-publication` floor is Unix seconds and is compared with
the file creation time. The CCL file contains packed, sorted 25-byte FASC-Ns from
a completed import. See [CCL provisioning](twic-ccl.md#indexed-storage). The
command reads the image into its own buffer and closes the file before opening
the card. It performs no network retrieval.

The root file assigns local trust. It must be an approved CA certificate; its
subject, public key and name constraints define the anchor. CA key usage and
path-length constraints are applied when present. Other critical root extensions
cause an input failure. Omit `--issuer` for a directly issued credential, or repeat
it for up to three untrusted issuer candidates. The root's own signature is
outside the credential path's signature checks.

The command selects the TWIC application and reads slot 9E. Legacy TWIC switches
to its PIV application for that operation. It accepts either registered PIV/TWIC
card-authentication purpose OID, validates the certificate path and identifiers,
checks the CCL, and verifies a fresh card challenge using native crypto. RSA-1024
requires `--allow-rsa1024`; NEXGEN requires RSA-2048. No PIN command is sent.

The system UTC clock supplies the evaluation time. After the card exchange, the
command checks CCL freshness and the certificate path again. Clock rollback or
an exchange taking over 30 seconds fails. Output contains status and fixed
failure descriptions, without credential identifiers. Exit status is 0 for
successful active-card authentication, 1 for a failed check or operation, and 2
for invalid arguments. Apply access authorization separately.

The host command reserves a 16 MiB CCL image and 8 KiB per provisioned
certificate. Its card-response, decompression and crypto workspaces are locked
in memory and cleared during cleanup; failure to lock them aborts the command.
Reader access is exclusive. Cleanup releases the transaction using
`SCARD_LEAVE_CARD`. Encrypted fingerprint-object authentication is optional.

### Include a signed CHUID

Add content-signer trust and revocation inputs to the authentication command:

```sh
build/credential-check/twic_authenticate \
  --reader 'Your PC/SC reader name' \
  --root /path/to/approved-card-root.der \
  --root-sha256 64-hex-digit-card-root-fingerprint \
  --ccl /path/to/provisioned-ccl.bin \
  --marsec-level 1 \
  --chuid-root /path/to/approved-content-root.der \
  --chuid-root-sha256 64-hex-digit-content-root-fingerprint \
  --chuid-issuer /path/to/content-issuer.der \
  --chuid-crl /path/to/content-root.crl \
  --chuid-crl /path/to/content-issuer.crl
```

Provision the content root and its SHA-256 fingerprint for this signing purpose.
The command keeps card-key
and content-signer trust separate. Omit `--chuid-issuer` for a directly issued
content signer; repeat it for up to three issuer candidates. Supply up to four
CRLs covering both the content signer's and card certificate's paths. Each path
uses its own configured trust source. The card path's revocation status is
checked with the final evaluation time. Missing, stale or unverifiable revocation
evidence fails validation. Certificates and the CCL are loaded before reader
access. CRLs are scanned after the CHUID supplies its signer candidates, using
bounded windows and a 64 MiB limit per file. Keep these files immutable during
the operation. The command retains parsed metadata and queried serial results
through its final time check; CRL signatures and signer trust are checked during
validation. Unsupported or oversized entries fail the operation.

After proving the card key, the command reads the CHUID from the selected
application: PIV for Legacy TWIC, TWIC for NEXGEN. A dedicated 16 KiB locked
buffer keeps it separate from the retained card certificate. Validation binds
its FASC-N and GUID to that certificate and checks the signature, content-signing
usage, trust, revocation and expiration. Both registered content-signing purpose
OIDs are accepted under the TWIC profile. The CHUID and certificate are checked
again at the final evaluation time.

Signed attributes use DER by default. `--chuid-ber` explicitly selects the
documented [TWIC BER attribute compatibility mode](cms.md). Unsigned CHUIDs fail
this requested signed-object check. CHUID expiration is inclusive through the
stated UTC calendar date.

CMS RSA signatures require explicit NULL algorithm parameters by default.
Use `--cms-rsa-parameters allow-absent` for cards that omit those parameters.
This policy applies to the requested CHUID, biometric and Security Object
signature checks and requires `--chuid-root`. Signature verification and trust
checks remain mandatory. `--cms-rsa-parameters null` selects the default policy.

`--chuid-root` requires `--chuid-root-sha256` and at least one `--chuid-crl`.
Exit status 0 requires both
active-card authentication and every requested CHUID check to succeed. The
command reports a separate signed-CHUID success line and clears its CHUID and
CMS workspace during cleanup.

### Authenticate encrypted biometric objects

Add `--tpk-hex /path/to/ZTA.txt` to the signed-CHUID command.
The file contains the hexadecimal ZTA field from an already-decoded TWIC
barcode; a trailing newline is accepted. It contains a privacy key, so keep it
outside the repository with access restricted to the reader application.
Enable `TINY_CRYPTO_ENABLE_AES=ON` and `TINY_CRYPTO_AES_ECB=ON` with a 128-bit
AES build. The option requires the content-signer trust and CRL inputs above.

The command loads the TPK into locked memory before opening the reader. After
card-key and CHUID authentication, it selects the TWIC application and reads
the protected fingerprint object. NEXGEN also requires its protected facial-image
object. The command retains each stored response and
decrypts a working copy of its `BC` value. It checks each CBEFF format and record,
binds the header and signed identifiers to CHUID, and validates each biometric
signature, signer path and revocation. An omitted signing
certificate selects the authenticated CHUID signer. Both registered purpose
OID namespaces and the selected BER policy apply.
The parser uses the version-3 CBEFF header specified by SP 800-76-1 and
SP 800-76-2. Signed entryUUID is required by default.

`--legacy-biometric-signature` explicitly selects the FIPS 201-1 section 4.4.2
signature profile referenced by the older TWIC biometric specification. It
permits an absent signed entryUUID. Signed FASC-N remains mandatory and must
match CHUID and the CBEFF header; a present entryUUID must match CHUID too.
Card application selection and BER compatibility are configured independently.

Each plaintext and stored response has a dedicated 16 KiB locked buffer
through the final time check. Both are wiped with the TPK and other credential
storage. The retained ciphertext preserves the input needed for security-object
hash verification. Success
requires every requested object check. The command validates fingerprint and
face record structure and the CBEFF validity period. Matching a person's live
sample remains an application check.
The utility sends no PIN commands and prints no key or biometric data.

### Collecting a security-object inventory

Add `--security-object` to `twic_authenticate` with the signed-CHUID trust and
CRL options. The command selects the TWIC application, collects the inventory,
binds its signed CHUID to the authenticated card identity, and verifies the
security-object signature, signer path, revocation and every stored-object hash.
These checks run again over the retained bytes at the final validation time.
With `--tpk-hex`, decryption reuses the collected biometric objects.
Each inventory object is read once.

Responses are bounded to 16 KiB per object. A locked pool reserves room for nine
responses plus transfer headroom. Decompression, card validation and signed-object
validation share scratch storage across their sequential phases. Encoded objects
and decrypted biometric records stay in separate buffers through the final checks.
All credential storage is wiped on exit. The command fails when a
required object is missing, an object exceeds its limit, or the signed hash list
differs from the collected inventory. Decrypting and interpreting the remaining
NEXGEN objects requires additional application handling.

`example_twic_inventory_read` in `examples/credential_io.c` selects and checks
the expected TWIC application, then reads its stored objects and security
object into a caller-owned pool. Legacy requires signed CHUID, unsigned CHUID
and fingerprints. NEXGEN also requires facial image and printed information;
iris, personal information and handwritten signature are probed as optional
objects. An optional object is absent only when GET DATA returns `6A82` with
an empty body. Other failures stop collection.

Set `max_object` to the largest accepted response and provide enough pool space
for retained responses plus transfer/status headroom. The returned inventory
borrows inner object contents from the pool, including ciphertext and TLV fields.
Its `security` span retains the complete `53` response. Only success writes the
inventory; processing failures wipe the pool and stop the IO session. Keep the
pool stable while passing its entries to `TC_PIV_security_validate`. Validate
the collected signed CHUID against the card identity before selecting its signer.
Use `TC_TWIC_unsigned_CHUID_validate` when container `3002` must also bind the
unsigned CHUID's identifiers and expiration through the authenticated inventory.

## Inspect both applications

TWIC CHUIDs use the signed profile by default. Append `--twic-unsigned-chuid`
to select the unsigned TWIC schema explicitly. The command reports those objects
as unsigned. PIV always requires its signed CHUID schema. Parsing exposes borrowed
identifier spans internally; the command prints no credential identifiers.

The example builds on macOS with the system PC/SC framework. Linux builds require
the PC/SC development package and `pkg-config`. The command disables core files
and locks its 16 KiB response buffer, 8 KiB decoded-certificate buffer and GZIP
workspace before opening the reader. Memory-locking
failure aborts the command. Output contains fixed object labels and status;
credential bytes are cleared after each object and before exit.

The connection is exclusive, with a transaction held across the operation.
Transport failures end processing. Responses share one exchange budget, and
partial objects stay within the response buffer. An oversized object fails with
a resource limit. Cleanup requests `SCARD_LEAVE_CARD`.

APDU construction and PC/SC access live in `examples/credential_io.c` and
`examples/credential_pcsc.c`. The command currently performs SELECT and GET DATA,
including GET RESPONSE and read-length correction. The separate PIN helper
requires a validated contact/PIV selection and a per-run guard retained across
connections; the command does not invoke it.

`test_twic_reader` exercises synthetic command/response scripts. On macOS,
`test_twic_pcsc` replaces the PC/SC service calls to test cleanup and transport
failures. These tests require no reader. The standalone example's CTest entries
exercise help and argument handling only.

## Card-key authentication

`card_key_policy.h` applies PIV/TWIC certificate-use and subject-key policy
without card commands. `credential_auth.h` supplies `example_card_check_key`
for the card-authentication key in slot 9E. Call it after validating the
certificate path and credential identifiers. Keep the reader transaction and
certificate buffer held throughout the check. `twic_authenticate` uses this
helper through the combined TWIC workflow.

The helper uses the generic `<tiny_crypto/key_challenge.h>` API to request a
fresh random digest, prepare the RSA representative or EC digest, and verify
the proof. Example code then sends GENERAL AUTHENTICATE and parses its 7C/82
response. The crypto API contains no APDU identifiers or card policy. Use a
cryptographically secure RNG. A failed RNG request ends processing before any
card command is sent.

PIV selection supports RSA-2048/3072 and P-256/P-384. RSA-1024 requires the
explicit `allow_rsa1024` policy. TWIC NEXGEN selects RSA-2048; TWIC Legacy uses
its separate PIV application for this operation. The application retains control
of certificate validity, legacy-algorithm policy, cancellation and authorization.

Supply `ExampleCardKeyWorkspace` in caller-owned storage. It contains a 64-byte
digest buffer, 384-byte RSA representative and 514-byte response buffer. The
helper clears this storage after processing. It also uses bounded command
storage on the stack. Provider scratch is supplied separately. Sharing either
workspace across concurrent calls requires application serialization.

RSA representatives use the library's `TC_RSA_encode_v15_digest` implementation.
The transport follows SP 800-73-5 Part 2 Appendix A.4 and TWIC Part 2 v5 section
5.3: short-command chaining, status-only intermediate replies and GET RESPONSE.
GENERAL AUTHENTICATE is never replayed for a 6C response. A failed chain stops
the session. The helper sends commands only to slot 9E and leaves the PIN alone.

PIV secure messaging follows the same transport boundary. The library returns
decoded handshake fields and protects caller-supplied authenticated spans. APDU
headers, `7C/81/82` handshake objects, `87/97/99/8E` protected objects, instruction
policy, command chaining and status words belong to the reader application. See
`examples/piv_sm_wire.h` for a bounded implementation of that framing.

`EXAMPLE_CARD_KEY_VERIFIED` reports possession of the supplied public key's
private counterpart. Preserve the separate trust and credential-policy decisions
when reporting the overall result.

### Certificate identifiers

`TC_PIV_card_identifiers_read` reads the complete DER `GeneralNames` value of
the slot 9E certificate's subject alternative name extension. Choose
`TC_PIV_CARD`, `TC_TWIC_LEGACY_CARD` or `TC_TWIC_NEXGEN_CARD` explicitly.
The result borrows the FASC-N, its encoded OID and the UUID URN from the
certificate buffer. Keep that buffer unchanged while using the result.

TWIC profiles accept the registered PIV and TWIC FASC-N OIDs. The reader
rejects duplicate identifiers, including duplicates using different OIDs.
FASC-N parity, delimiters and decimal fields are checked. PIV requires a version
1, 4 or 5 UUID. NEXGEN requires the Appendix D namespace and a UUID card number
matching the FASC-N's agency, system and credential fields. Select NEXGEN for
the TWIC application's certificate. Legacy TWIC permits an absent UUID or the
nil UUID.

Pass the borrowed 25-byte `fascn` directly to `TC_TWIC_CCL_contains`, or use
`TC_PIV_card_identifiers_match` to compare the identifiers with a CHUID's
FASC-N and GUID. The latter returns its comparison through `matched`; check
both the operation status and that value. An absent legacy UUID matches a nil
GUID. Certificate trust, object signatures and CCL freshness remain separate
workflow decisions.

For a slot 9A certificate, call
`TC_PIV_authentication_identifiers_read` with the Card UUID from the signed
CHUID. The function uses that value to select the Card UUID when the SAN also
contains the optional Cardholder UUID. It returns both borrowed URNs and
requires the Cardholder UUID to use version 4. A TWIC reader can call
`TC_TWIC_authentication_identifiers_read` for the same selection with the
registered PIV/TWIC FASC-N OID pair and the permitted absent Card UUID.

### TWIC active-card authentication

`credential_validate.h` combines the checks in TWIC Part 2 section 7.5 through
`example_twic_authenticate`. Supply an `ExampleTWICRequest` with a
trust source, path policy, selected TWIC profile and an acquired CCL snapshot.
Supply the slot 9E certificate as DER. Path discovery searches the source's
issuer candidates and validates against its provisioned anchors. The request
borrows all certificate and store data.

The path policy's purpose must be the registered PIV or TWIC card-authentication
OID used by the certificate. The helper requires digital-signature key usage and
an explicit extended-key-usage match. It reads the validated certificate's
identifiers, checks CCL freshness and cancellation, then proves possession of
the card key. A second CCL query catches a publication that supersedes the held
snapshot during card I/O. The executable checks the snapshot again after signed
object validation and before its final certificate path check.

Use `TC_TWIC_NEXGEN_CARD` after selecting the NEXGEN TWIC application. Use
`TC_TWIC_LEGACY_CARD` after selecting the Legacy card's PIV application. Keep the
reader transaction and acquired snapshot held until the operation returns, then
release the snapshot. Store operations need the application's usual locking.

The helper converts the path's evaluation time to Unix seconds for CCL freshness;
this workflow accepts evaluation dates from 1970 onward.
Supply `ccl_max_age` in seconds and the persisted `ccl_minimum_publication` floor.
Apply the application's maximum transaction
duration and clock policy before acting on a result. CCL publication metadata
must come from trusted provisioning. Each lookup has its own `ccl_reads` bound;
certificate processing and key proof share the supplied work budget.

`EXAMPLE_TWIC_AUTHENTICATED` reports successful active-card authentication at that
instant. Other outcomes distinguish cancellation, stale or unavailable status,
invalid credentials, unsupported algorithms, resource limits, transport failure
and API/provider errors. Apply access authorization separately. The executable
composes this operation with `TC_PIV_CHUID_validate` when content-signer inputs
are supplied. Its accepted result supplies authenticated identifiers and the
signer certificate to Security Object and biometric validation. Biometric sample
matching and privacy-key handling remain application responsibilities.

The helper uses caller-owned `ExampleTWICWorkspace` and clears it after
processing. Keep it outside a small task stack, disjoint from request data and
crypto-provider scratch. The `twic_authenticate` command supplies these inputs
from its provisioned files and selected card.

### Provisioned inputs

`pki_input.h` provides the example utilities' bounded file reader and array-backed
trust source. `example_read_file` reads into caller storage, rejects empty or
oversized files, and clears the buffer on read failure. Its returned span borrows
that buffer. `example_read_stream` provides the same checks for an open stream.

Use `example_x509_source` with separate arrays of issuer candidates and locally
authorized anchors. Keep the arrays, encoded certificates and adapter context
unchanged during validation. Populate anchors through the application's trusted
provisioning process; loading an issuer candidate gives it no anchor authority.
The CMS executable uses these same helpers.

### Synthetic authentication tests

With `TINY_CRYPTO_TEST_OPENSSL=ON`, run:

```sh
cmake --build build --target test_card_authentication
ctest --test-dir build --output-on-failure -R '^test_card_authentication$'
```

The munit suite generates its own issuer and card keys in memory. The library
validates the generated certificate's path, time and card-authentication usage,
reads its FASC-N and UUID, checks identifier binding and exercises synthetic CCL
lookup, then verifies replies signed by an OpenSSL-backed synthetic card. Test cases
cover all five key sizes/curves, altered and replayed signatures, malformed
responses, failed command chains, transport failure at each exchange, exhausted
exchange budgets and partial RNG failure. The fixtures contain no live-card data;
generated private keys are freed at the end of each test.

The combined TWIC workflow runs with both registered card-authentication purpose
OIDs. Cases cover cancellation, stale and missing lists, source errors, exhausted
lookup budgets, expired or altered certificates, an unrelated root, altered
challenge replies and a CCL publication during the exchange. Rejected
pre-challenge checks assert that neither the RNG nor card transport was called.

On macOS, `test_twic_command` exercises the executable's workflow with synthetic
applications and objects. It replaces the reader and memory-protection calls,
checks cleanup and buffer wiping, and covers failures at each transfer boundary.

`test_twic_authenticate_command` runs the authentication executable with synthetic
files and PC/SC services. It covers NEXGEN, Legacy fallback, both purpose OIDs,
issuer discovery, cancellation, certificate/reply tampering, entropy and transport
failures, clock rollback, elapsed-time limits, and CCL/certificate expiry during
the exchange. Compressed fixtures cover successful decompression, bad CRCs and
truncated streams. Root cases check CA status, key usage, path limits, permitted
name constraints, unsupported critical extensions, and a same-name root with
an unrelated key. A separate card key tests a valid certificate paired with the
wrong signing device. Every cleanup checks
that protected credential storage was cleared.

Signed-CHUID command cases cover Legacy/NEXGEN applications, either content-signing
purpose, direct/intermediate issuers, revoked and untrusted signers, wrong-card
objects, signed signer-name mismatches, tampered signatures, unsigned objects,
bad CRLs, read failures and expiration during the transaction. Argument tests
reject incomplete trust inputs and excess issuer/CRL files before reader access.
The BER case signs reversed attribute order with an independent fixture key.
It succeeds with `--chuid-ber`, fails under DER policy, and rejects a changed
signature with compatibility enabled.
RSA fixtures cover omitted CMS algorithm parameters in CHUID, biometric and
Security Object signatures. Each requires `--cms-rsa-parameters allow-absent`;
changed signatures or signed content fail with that policy enabled.
