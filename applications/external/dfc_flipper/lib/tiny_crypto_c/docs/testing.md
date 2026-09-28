<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Running the tests

Run commands from the repository root. You need CMake, Make, a C compiler,
a C++11 compiler, and Python 3. Use a separate build directory for each compiler
or sanitizer configuration. Independent crypto cross-checks require OpenSSL 3
development headers and libraries.

Start with `make test`. Use [checked-in suites](#checked-in-suites) for full vector
and sanitizer runs, or [external vectors](#cryptographic-vectors) to include the
pinned archives and capture corpora. The sections below describe focused suites.

The OpenSSL-enabled `test_card_authentication` target exercises synthetic
card-certificate validation followed by a fresh 9E challenge and native signature
verification. It covers RSA-1024/2048/3072, P-256/P-384, replay, altered replies,
framing errors, RNG failure, transport failures and exchange limits. See the
[credential reader guide](credential-reader.md#synthetic-authentication-tests)
for the focused command. Generated keys and certificates stay in test memory.

On macOS, the OpenSSL-enabled `test_twic_authenticate_command` target uses the
SDK's zlib to create compressed fixtures and tests the
host command through synthetic PC/SC and provisioned-file inputs:

```sh
cmake -S . -B build -DTINY_CRYPTO_TEST_OPENSSL=ON
cmake --build build --target test_twic_authenticate_command
ctest --test-dir build --output-on-failure -R '^test_twic_authenticate_command$'
```

It exercises application selection, certificate retrieval, path discovery, CCL
checks, card proof and cleanup. Cases include compressed certificates, root
CA/key-usage/path/name constraints, expiry during the exchange, and argument
rejection before reader access. These tests use no connected credential.

## Checked-in suites

TWIC CCL parsing and lookup use `test_twic_ccl`. See the
[CCL guide](twic-ccl.md#tests) for testing a downloaded TSA feed. Its external
file case skips when `TC_TEST_TWIC_CCL` is unset; synthetic cases always run.
When supplying a file, set `TC_TEST_TWIC_CCL_MD5` to its published 32-digit
hexadecimal checksum. `test_md5_0` and `test_md5_1` exercise the optional
[MD5 implementation](md5.md) with both zeroization settings.

```sh
make test
make test-full
make test-sanitize
make test-sanitize-full TINY_CRYPTO_TEST_FULL=ON
make test-msan CC=clang CXX=clang++
make test-msan-full CC=clang CXX=clang++ TINY_CRYPTO_TEST_FULL=ON
```

`test-full` includes the checked-in NIST CAVP and Wycheproof vectors. External
corpora below require their pinned archives. `test-sanitize` runs the core suite
with AddressSanitizer and UndefinedBehaviorSanitizer. `test-sanitize-full` also
runs tests labelled `extended`, including exhaustive RSA and corpus cases. The
MemorySanitizer targets use the same core and full split on Linux with Clang.

Push CI runs the core sanitizer suite with GCC and Clang. The **Full test suite**
GitHub Actions workflow runs the pinned Wycheproof and NIST ECDH archives in
release and sanitizer builds when started through `workflow_dispatch`.

Use `act` to run the push sanitizer checks in Linux. `--bind` includes current
working tree changes:

```sh
act workflow_dispatch --bind -W .github/act/cms-sanitizer.yml -j gcc
act workflow_dispatch --bind -W .github/act/cms-sanitizer.yml -j clang
act workflow_dispatch --bind -W .github/act/cms-sanitizer.yml -j msan
```

Several corpus readers skip when run directly without an input file. Their
runner tests supply the vectors: `test_wycheproof_ecdsa`,
`test_wycheproof_rsa_signatures`, `test_wycheproof_rsa_oaep`,
`test_wycheproof_aead`, and `test_cms_corpus`. Check that the relevant runner
is configured and passes before treating a standalone reader skip as expected.
The Wycheproof runners report accepted input categories and excluded parameters.

C tests use [µunit](https://nemequ.github.io/munit/); C++ tests use
[doctest](https://github.com/doctest/doctest). CTest also checks installed
consumers, package boundaries, and resource-profile configuration.
The installed-consumer check builds a separate Release library, installs it,
and compiles isolated C99 and C++11 callers against that installation. It also
builds the X.509, CMS, TWIC validation and RSA encryption examples. The isolated
callers exercise supported error paths from both languages. The RSA check
verifies RNG failure, unchanged ciphertext and scratch cleanup through the
installed API.
Each optional module also has a smallest-supported C and C++ umbrella-header
compile target, which catches accidental dependencies on unrelated features.
These package checks are separate from sanitizer-instrumented unit tests.
The install manifest is checked against the expected library, headers, CMake
package files and license notices. Both installed license files must match the
source copies byte for byte.
Direct-source checks compile and link every product `.c` file in AES-only,
TLV-only, and EC-only builds, with SHA-256 disabled.

To run just the installation checks after configuring a build:

```sh
ctest --test-dir build -R '^test_installed_consumer' --output-on-failure
```

The validation workspace test checks arena sizing, profile capacities, stable
workspace metadata and context alias rejection:

```sh
cmake --build build --target test_validation_workspace
ctest --test-dir build -R '^test_validation_workspace$' --output-on-failure
```

## External vectors and capture replay

### CMS parsing

`test_cms_reader` covers public envelope parsing, attached and detached content,
version consistency, truncation, exact work budgets and every pair of overlapping
storage ranges. It also runs the fixed-workspace example, which is built against
the installed library with both C and C++ callers.
Signer iteration covers definite/indefinite collections and members, exact shared
budgets, later-member errors, unchanged reader/output on failure, and zero-work
exhaustion. The overlap cases also cover the iterator's managed state.

The [CMS API](cms.md) tests cover strict DER and explicitly selected BER
definite-order attributes, malformed values, duplicate/missing attributes,
resource limits, and borrowed signature input. Signature verification has its
own tests below.

Configure `TINY_CRYPTO_TLV_CORPUS` with the parser corpus root to add
`test_cms_corpus`. It extracts CMS from 70 captured CHUIDs, 69 Security
Objects and 111 biometric objects, then checks envelope versions, content types,
signer identifiers and
signed attributes through the public readers. Both explicit attribute encoding
modes are exercised, along with every truncated envelope prefix. These checks
cover syntax; captured tampered signatures still require cryptographic checks.
The biometric adapter uses the CBEFF lengths from SP 800-76-2 Table 14.
Card 55's face signature contains an empty `entryUUID`; both encoding modes
must reject its signed attributes and leave the output unchanged.
The biometric signature group checks the original CBEFF header and record with
the library's native crypto and each card's CHUID signer certificate. It covers
107 accepted signatures, two tampered records, the empty UUID and an unresolved
signer. Each accepted record is also tested with changed content. Strict and
omitted-RSA-parameter policies run with both attribute encoding modes.
The content and precomputed-digest APIs must agree; changing the supplied digest
must fail verification. Installed C99 and C++11 consumers call both policy APIs.
The PIV profile reader is checked with exact and one-unit-short work budgets,
overlapping input/output storage, and both namespace and attribute modes.
Installed C99 and C++11 consumers also call `TC_PIV_CMS_read`.
Captured biometric FASC-N/UUID attributes are compared with each card's CHUID:
98 pairs match, 12 differ, and one UUID is malformed. Signature and identifier
verdicts are recorded separately, including signed objects with mismatched IDs.
Header FASC-N values match the CMS attributes in the captures; eight differ from
the CHUID. These comparisons use the CBEFF reader's borrowed header span.
Native path fixtures use EC and RSA signers with issuer/serial and key identifiers,
attached and detached content, and empty messages. RSA fixtures also omit the CMS
algorithm parameter: strict policy rejects them, while explicit compatibility
permits path construction and retains leaf and intermediate revocation checks.

The native CMS suite also signs a CHUID fixture with OpenSSL and verifies the
two borrowed content spans for PIV and signed TWIC. Changing the card UUID or
omitting the trailing `FE` field must fail signature verification.
OpenSSL-generated variants include PIV and TWIC FASC-N attributes, with and
without entryUUID. Their signatures verify with the original attributes and
fail when either identifier changes while detached CHUID content stays unchanged.
Parsed identifier values are compared with their corresponding CHUID fields.

CMS path tests also cover detached content split across borrowed spans, empty
spans and messages, altered content, exact work budgets, combined byte limits,
descriptor-count overflow, and overlap with writable state. Attached envelopes
reject application-supplied content spans.

The combined credential tests check CRL metadata and encoded bytes against CMS
scratch, retained path spans, CRL cache and revocation nodes. Preflight failures
preserve work and scratch. Source-callback tests return certificate spans into
each scratch area and require rejection on the first callback, with the store
snapshot released on return.

```sh
cmake -S . -B build -DTINY_CRYPTO_TLV_CORPUS="$PWD/tests/vectors"
cmake --build build --target test_cms_corpus_reader
ctest --test-dir build -R '^test_cms_corpus$' --output-on-failure
```

`test_cms_content` checks public SHA-1/SHA-2 content hashing and digest binding:
segmented content, digest/type mismatches, invalid digest sizes, shared read-only
buffers and work limits. BER cases cover primitive and nested OCTET STRINGs,
every truncated prefix, malformed chunks, short output buffers and untouched
spare bytes. `test_cms_reader` checks disabled hashes and overlapping storage.

`test_cms_verify` checks the public prehashed signer API's work limits, storage
overlaps, provider budget enforcement and rejection of non-data content without
signed attributes. Native cases cover digest binding, explicit attribute encoding,
RSA signatures without attributes, fragmented ECDSA signatures and short scratch.
`test_cms_verify_content` uses the same tests with hashing enabled. It checks raw
and BER input, empty messages, every short work budget and truncated BER prefix,
raw length bounds, and explicit format selection. Both builds check all storage
overlap cases across prehashed, raw and BER verification. PSS native cases also
exercise raw and chunked content with matching and distinct attribute hashes.

`test_cms_signer_info` exercises the public reader with definite and indefinite
SignerInfo fields,
chunked key identifiers and signatures, issuer/serial identifiers, field order,
truncation and work limits. Issuer Name tests reject malformed RDNs, OIDs and
attribute strings. Mixed BER/DER Name comparisons cover unsorted attributes,
nonminimal lengths, duplicate attributes, malformed tails and work limits.
Constructed string cases split UTF-8, BMP and UniversalString characters across
nested OCTET STRING chunks and check normalization against primitive DER names.
Certificate identifier tests cover issuer/serial and primitive or constructed
SKI values, missing or duplicate SKI extensions, mismatches and malformed tails.
Candidate iteration tests cover embedded/external ordering, byte and record
limits, truncated collections and callbacks that violate the source contract.

`test_cms_external_collections` checks external certificate and CRL iteration,
borrowed records, collection boundaries and exhausted limits. Certificate and
CRL validation have separate suites.

```sh
ctest --test-dir build -R '^test_cms_external_collections$' --output-on-failure
```

`test_cms_children` checks envelope framing, digest AlgorithmIdentifier syntax,
unknown algorithm OIDs, work budgets, and the SignedData version rules
from RFC 5652 section 5.1, including precedence between certificate types,
revocation formats, content types and signer versions. Certificate and CRL
body validation is separate from these version tests.
The same suite checks OtherCertificateFormat and OtherRevocationInfoFormat:
required OID/value pairs, nested BER values, missing or extra fields, truncation,
and work limits. Values remain borrowed; their format-specific contents are not
interpreted by these readers.

```sh
ctest --test-dir build -R '^test_cms_(reader|attributes|algorithm|signer_info|children|octets|content|verify|verify_content|pss|native)$' --output-on-failure
```

With OpenSSL tests enabled, `test_cms_native` connects BER content hashing,
attribute binding and native ECDSA verification. It covers DER attributes and
explicitly selected unsorted attributes and nonminimal definite lengths,
changed content and changed signatures.
`test_x509_native` compares message and prehashed ECDSA, RSA v1.5 and RSA-PSS
verification against OpenSSL, including changed digests/signatures and wrong
PSS salt lengths. `test_x509_signature` checks provider dispatch, absent callbacks,
invalid callback results, budget increases and short work budgets without native
hashes.
It also checks CMS `rsaEncryption` signatures with 1024-, 2048-, and 3072-bit
keys against OpenSSL. `test_cms_algorithm` covers digest selection, PSS key
restrictions and mismatched algorithm identifiers. It checks BER NULL and PSS
parameters, malformed encodings, and parser budgets. ECDSA requires matching
content/signature hashes; PSS allows distinct hashes when signed attributes
are present. See [RFC 5753 section 2.1.1](https://www.rfc-editor.org/rfc/rfc5753.html#section-2.1.1)
and [RFC 4056 section 3](https://www.rfc-editor.org/rfc/rfc4056.html#section-3).
`test_cms_pss` runs the public SignerInfo parser and verifier against OpenSSL
PSS signatures at all three RSA sizes. Cases include matching hashes, SHA-256
content with SHA-384 signed attributes, different MGF/signature hashes, and
signatures without attributes. PSS-restricted SPKIs exercise hash, MGF and
minimum-salt constraints. Negative cases change the content digest, signature,
hash, MGF hash or salt length; exact and short work budgets are checked too.
The generated CMS cases connect envelope and SignerInfo parsing to certificate
identifier matching, content binding and signature verification. They cover
issuer/serial and key-ID identifiers, nonmatching certificates and repeated
matches across embedded and external sources. Generated ECDSA signatures are
split at every byte boundary in nested BER OCTET STRINGs, parsed as SignerInfo,
and verified with the native provider. These cases also check corrupted
signatures and short scratch buffers.

`test_cms_native` also checks the public signer path builder with issuer/serial
and key-ID signers, an embedded intermediate and an explicit external anchor.
Cases cover missing intermediates/anchors, expired certificates, changed content
and signatures, wrong identifiers and missing providers. Candidate retries retain
unsupported/limit results and stop on provider errors. Public checks cover exact
and short budgets, index/record/byte capacities, all writable ranges overlapping
input, and source records overlapping the certificate index. The compiled example
is exercised with a valid chain and by installed C/C++ consumers.
Complete SignedData policy and revocation validation remain separate.

The combined credential example is exercised by `/cms/native/embedded-path`.
It uses a held trust snapshot and signed issuer/root CRLs for clear chains,
revoked signers and revoked intermediates. Negative cases cover missing CRLs,
wrong or absent trust anchors, expiration, wrong key usage, changed signatures,
missing signers, mismatched policy times, unheld snapshots and exhausted work.
These run with both signer-ID forms and attached, detached and empty content.

The public envelope path API runs against attached, detached and empty messages
with both signer-ID forms. Tests check expected content type, missing signer
indices, explicit second-signer selection, absent/NULL/BER NULL digest parameters,
unknown extra digest identifiers and missing or invalid digest-list entries.
They reject replacement content for attached envelopes and incorrect detached
messages. Exact/short work budgets and storage-overlap checks cover the combined
operation. RSA-PSS cases exercise the same workflow at 1024, 2048 and 3072 bits,
including restricted keys and distinct content/signed-attribute hashes.
The parser fuzzer also calls the envelope path API with bounded work and no trust
anchors, checking that failures preserve the result and cannot establish trust.

### X.509 signatures and path checks

`test_source_hash` checks resumable SHA-256 hashing, read failures, work limits
and output preservation. Its optional CRL case reads a DER CRL from disk through
a 1 KiB window, loads bounded metadata, checks every entry's syntax and extension
policy, and verifies the signature with the supplied issuer certificate. Signer
checks cover the issuer name, `cRLSign` usage and algorithm/key restrictions.
Negative cases alter a signed byte, select a different hash or change the signer
name.
Issuer trust, revocation scope and entry policy have separate tests.

```sh
cmake --build build --target test_source_hash
TC_CRL_FILE=/path/to/list.crl TC_CRL_ISSUER=/path/to/issuer.der \
  ./build/test_source_hash
```

CRL indexing tests check borrowed storage, source errors, retained extension-policy
failures, row capacity, and exact work budgets. Empty and other-format-only
collections require no CRL rows. Failed indexing preserves the reader and output.
Indexed delta tests cover source-order pairing, skipped records, multiple matches,
and iterator/output preservation on exhausted work budgets.
Guarded revocation sources reject records that overlap parser scratch, reader
state or output. Exact and short work budgets check failure preservation.
Index storage tests cover embedded/source records overlapping rows, OID and frame
scratch, work counters and output, plus overlaps between writable arrays.

`test_x509_crl` covers DER CRL fields and revoked-entry iteration: v1/v2 rules,
optional dates and extensions, algorithm mismatches, malformed fields,
truncation, and parser budgets. Extension checks cover embedded DER framing,
duplicate OIDs, scratch capacity and work limits. Value tests cover CRL/base
numbers, all one-byte reason codes, invalidity dates, authority key identifiers,
and issuer GeneralNames, including names nested in AKIDs. Issuing distribution
points cover full and relative names, reason flags, DER booleans, field order
and mutually exclusive certificate scopes. Distribution-point lists check
repeated entries, issuer names, reasons, field order and iterator state on
errors. The separate distribution-point issuer check requires one directory
name and rejects empty, mixed or repeated names. Issuer-linkage tests cover
normalized certificate issuer names, exact explicit issuer encodings, and the
indirect-CRL requirement. CRL FreshestCRL values permit
names only. Relative-name comparison tests use a borrowed base Name and RDN,
checking equivalence with complete Names, normalization, short work budgets
and scratch overlap. Scope-name tests cover full/relative combinations,
multi-name intersections, issuer fallback, different issuer bases and malformed
trailing names. Combined scope tests cover CA/user/attribute restrictions,
all four reason-mask combinations, disjoint reasons and the unused bit.

Freshness tests cover both update boundaries, missing deadlines, reversed
intervals, leap days and the 2049/2050 transition. Refresh is due at nextUpdate;
there is no implicit clock-skew allowance.
Extension metadata tests cover every criticality combination for the supported
CRL fields, unknown critical OIDs, duplicate rejection and output preservation.
Entry metadata checks cover reason codes, invalidity dates and issuer names,
including absent values, criticality combinations and unknown critical OIDs.
Extension-policy tests reject unknown critical extensions, require critical delta
and issuing-distribution-point extensions, require noncritical CRLNumber and
FreshestCRL, and reject FreshestCRL on a delta. Entry-policy tests require
noncritical reason/invalidity-date extensions, critical certificateIssuer on
indirect CRLs, and delta CRLs for removeFromCRL. Inheritance tests scan five
entries through the default issuer, two replacements and inherited issuers,
checking that errors and work limits preserve iterator state. Signature and
trust integration remain unfinished. Entry matching checks serials first,
then normalized default issuer names or exact explicit issuer encodings; the
tests exercise each inherited issuer, mismatches and work limits.
Whole-CRL lookup tests cover missing matches, duplicate matches and malformed
entries after a match, with output preserved on failure.
Delta-pairing tests cover number-range boundaries, wide integers, issuer
normalization, IDP/AKID presence and value mismatches, and unknown critical OIDs.
Base/delta result tests cover every one-byte reason code with present and absent
matches, delta precedence, removeFromCRL, and preservation of reported dates.
Signer KeyUsage tests cover cRLSign, unrelated bits, an absent extension,
duplicates, malformed values and short work budgets.
Combined coverage tests give future or stale CRLs, and those without
nextUpdate, no eligible reasons. Current CRLs still pass extension policy, certificate-type
and distribution-point checks. Tests include exact and exhausted budgets and
unchanged output on failure.
Evidence-accumulation tests cover every partition of the eight reason bits,
duplicate coverage, terminal results, revocation precedence and invalid state.
They reject raw removeFromCRL entries and accept the resolved base/delta result.
Authority-identifier tests cover SKI and issuer/serial hints, absent identifiers,
name normalization, conflicting hints, alternative issuer names and malformed
tails. Directory names are supported; unresolved other name forms return
`TC_TLV_UNSUPPORTED`. Candidate matching does not establish trust.
`test_cms_native` extracts OpenSSL-generated CRLs from CMS,
reads their entries, and verifies their signatures with the native provider.
Signer checks cover subject linkage, cRLSign permission, altered signatures,
missing providers and short work budgets. These checks use the signer
certificate's public key; they do not establish its trust path.
Selected-CRL lookup tests verify complete and delta signatures with the same
signer key, then check entry overrides and removals. They reject a delta signed
by another key despite matching authority identifiers, incompatible numbers,
altered base signatures and incomplete delta arguments. Exact and short work
budgets check that failures leave the result unchanged.
Pair-validation tests check both signatures and the signer path without scanning
entries, including wrong-key rejection and exact/short work budgets.
Delta-preference tests authenticate the base, then mix valid and wrong-key deltas
in different orders. They check highest-number selection, expired records,
missing providers, budget exhaustion, and conflicting signed records at the
highest number. Older conflicts do not hide a newer valid delta.
Signature-cache tests check valid/invalid reuse, capacity and initialization
budgets, cached delta selection, and retries after provider errors or limits.
Effective-candidate iteration covers all delta policies across base/delta update
intervals, cached signature reuse, iterator preservation and exact/short budgets.
Latest-number tests compare complete CRLs with effective base/delta numbers in
reordered sources, preserve output on short budgets, and report unnumbered
authenticated candidates as unsupported by numbered selection.
Scope application compares the target's revocation information across tied
complete/base-delta candidates. Tests accept matching results and reject differing
reasons or update times in either source order without changing evidence.
Conflicting signed deltas cannot hide a newer complete CRL. Without that newer
record, the same conflicts reject selection and leave evidence unchanged.

Explicit thisUpdate ordering covers numbered and unnumbered complete CRLs in both
source orders, including a newer CRL with a different revocation reason.
Equal-time candidates must agree on revocation information. Tests also check
short/exact work budgets, invalid ordering options, and signature-cache reuse.

Scope processing includes signer discovery and path validation to the selected
anchor. Tests use complete and delta reference CRLs, verify each CRL signature
once, reject the wrong anchor and short state storage, and preserve outputs when
work runs out. Terminal evidence returns without another search.
Already-covered reasons and unrelated issuers skip signer discovery without
changing cache states. Current deltas remain usable with stale base CRLs.
Signed reason-partition tests combine two scopes under one work budget in both
source orders. Partial coverage stays undetermined; a bad signature cannot fill
the missing reasons. A valid revocation remains decisive in either order.
Key-rollover fixtures use two trusted signer keys with the same issuer and IDP.
They check both source orders, conflicting CRL numbers, a damaged newer signature,
and unnumbered CRLs under number and explicit update-time ordering.
Number-order cases also run with exact and one-unit-short work budgets; exhaustion
must preserve caller evidence.
Base/delta rollover tests try all six record orders under each delta policy,
including a damaged delta signature and required-delta rejection.
Conflicting older CRLs are superseded only by an authenticated newer CRL, in
each record order.
Point-runner tests check staged contributions and every contributing signer path.
Distribution-point-list tests cover split reason masks, issuer fallback, an absent
extension, malformed trailing points, input/scratch overlap, and exact/short work
budgets. Malformed lists are rejected before signer callbacks run.
Issuer-DN fallback tests cover matching and nonmatching named CRLs, including
every insufficient work budget and an exact-budget retry.
Certificate-driver fixtures verify target paths, then test listed points and
issuer-alternative-name fallback against a signed CA-only CRL. They cover CA
filtering, wrong locators, duplicate extensions, malformed values, exact/short
budgets, and extension data overlapping signature-state storage.
They also identify a revoked target through issuer-alternative-name fallback.
Cross-point tests reject signer and source failures after a first contribution
without publishing partial evidence.
Direct-anchor signature tests cover the selected key and issuer name, damaged
signatures, unavailable providers, and exact/short work budgets.
Signer-check callbacks receive the effective base/delta buffers. Rollover tests
check those borrowed pointers in every record order and exclude damaged deltas.
Unresolved signer tests cover older/newer and equal-number candidates, definitive
rejection, and alternate certificates for the same key. Exact/short budgets also
check that unresolved evidence is not published.
Dependency-worklist tests cover direct-anchor decisions, a delegated issuer,
revoked issuers, an unresolved cycle, node exhaustion, source errors, and
exact/short work budgets. Node/state overlap is rejected before scratch changes.
Delegated chains run with both CRL orders and with each CRL omitted separately.
Selected-path tests check an unrevoked chain, revoked issuer and leaf indexes,
revocation reason/date, missing evidence, and exact/short budgets. Invalid
arguments and paths or results overlapping scratch fail without publishing output.
Path members share verified dependency nodes. Repeated calls reuse the same
scratch with changed CRL indexes to check that prior results are not retained.
The public CRL reader is exercised by the CRL corpus, truncation, budget and
argument tests. CMS and CRL readers share their buffer-boundary checks.
Public CRL extension tests cover presence/criticality combinations, unknown
critical OIDs, duplicate extensions, budgets and overlapping workspace arrays.
Public index tests cover borrowed records, empty collections, malformed later
records, capacity/work limits and later input overlapping record storage.
Public path-revocation tests cover unrevoked paths, revoked issuers/leaves,
missing required inputs, candidate-byte and work limits, and caller metadata/work
overlap before source reads. They check source-error propagation, rejection of
callbacks that increase the work budget, and unchanged results on failure.
The caller-owned workspace example is compiled and exercised too.

Run the focused certificate-iterator and revocation-workspace tests with:

```sh
ctest --test-dir build -R '^test_x509_(candidate|revocation)$' --output-on-failure
```

These cover source limits, borrowed buffers, workspace overlap, dependency
resolution, and held-path results. Run the signed native revocation cases with:

```sh
./build/test_cms_native /cms/native/revocations
```

The `group` parameter selects `signer`, `discovery`, or `selection` checks.
The `outcome` parameter selects `clear` or `revoked`; omit it to run both.
For example, to check signer handling with a CRL containing no revoked entries:

```sh
./build/test_cms_native /cms/native/revocations --param group signer --param outcome clear
```

Source errors injected after an issuer is resolved preserve the target output
and stop further source reads.
Tests reject a missing check, callback failures and increasing work, including
failure after one partition succeeds. Source failures stop the run; rejected
alternatives cannot leave partially published evidence. Empty indexes and partial
coverage are tested separately from a determined revocation status.
The shared failure cases cover provider retries, source errors, malformed
certificates, stale CRLs, and callbacks that increase the work budget.
Storage tests place cache bytes in input metadata and scratch arrays, and return
certificate/anchor bytes overlapping each writable range. They reject partial
frame-array overlaps while allowing shared tree/path frames.

CRL scope-group tests compare normalized issuer names and issuing distribution
points, independently of authority-key hints, with exact and short work budgets.
Staged application tests apply the authenticated selection without another
signature check. They cover entry overrides/removals, expiry, completed evidence,
and unchanged evidence when the work budget is exhausted.
Indexed processing tests cover complete-only, optional-delta and required-delta
policies before and after the complete CRL expires. Signature-call counts check
that the base and signer path are not verified again after delta selection.
Overlapping validity intervals check that a selected delta takes precedence over
a still-current complete CRL. Indexed signer retry also covers provider failures,
wrong anchors, malformed candidates, source errors and growing work budgets.
Combined processing tests add evidence only after freshness, scope, signatures
and the signer path pass. They cover partial reason coverage, terminal results,
wrong anchors and preserved evidence on failure. A current delta can update a
stale complete CRL. The indexed-record path exercises both correctly signed
deltas and matching-metadata deltas signed by the wrong key. A future or expired
delta contributes no evidence. Provider
call counts check that each CRL signature is verified once during processing.
An unsupported critical entry checks processing order: rejected signer paths
stop before entry lookup, while a trusted signer reaches the entry-policy error.
Discovery tests search embedded and external certificates, skip candidates
whose KeyUsage excludes cRLSign, and reject conflicting authority hints.
They also check exact work budgets, iterator/output preservation on failure,
and invalid filter results, including callbacks that increase the work budget.
Combined signer-trust tests verify the CRL signature and signer path under one
selected anchor. They cover wrong anchors, expired signers, absent and
disallowed KeyUsage, altered signatures, missing providers, and shared work
limits. Failures must leave the result unchanged.
Signer-search and processing tests retry after an expired certificate or an unsupported
provider operation, skip disallowed KeyUsage, and stop on provider errors.
They check that budgets include failed attempts and that the input iterator
and prior evidence remain unchanged on failure. Source errors and malformed
records stop the search. Terminal evidence needs no further candidate reads.
Delta processing also finds its signer in the CMS certificate collection.
Certificate-index tests combine embedded and external candidates, fetch each
external record once, and check capacity, record, byte and work limits. The
native path test needs an intermediate available only inside the CMS object;
its successful path borrows that certificate from the original buffer.
The same fixture exercises CRL signer discovery through that embedded chain
and rejects an altered intermediate signature.

Revocation-record tests use the same bounded collection reader as certificate
discovery. They cover embedded CRLs, OtherRevocationInfoFormat, external records,
exact and short work budgets, byte limits, malformed choices, truncation and
callback failures. An exhausted reader returns `END` with zero work and leaves
its state and output unchanged. Native tests feed a CRL through embedded and
external record sources before parsing and checking its signer path.

```sh
ctest --test-dir build -R '^test_(x509_crl|cms_native)$' --output-on-failure
```

The CRL corpus suite covers 439 fixtures: 12 synthetic, 173 PKITS, 199 from
NIST's 2001 suite, and 55 ICAM CRLs. It rejects the synthetic algorithm mismatch
and a PKITS RSA signature with a nonzero unused-bit count. Other bad signatures
can remain structurally parseable. The file named `crl_v1_no_extensions.pem`
encodes v2; the test follows its bytes, not its name. Fixture hashes and group
counts are checked before running the reference cases.

```sh
cmake -S . -B build -DTINY_CRYPTO_TLV_CORPUS="$PWD/tests/vectors"
cmake --build build --target test_x509_crl
ctest --test-dir build -R '^test_x509_crl_corpus$' --output-on-failure
```

`test_x509_native` checks the [native provider](x509-crypto.md) against
OpenSSL-generated ECDSA, RSA v1.5 and PSS signatures, including segmented
messages, changed content, exhausted budgets and scratch overlap.
It also exercises certificate verification and the client-path example with
explicit EC/RSA trust anchors, rejecting expired or tampered leaf certificates
and the wrong anchor key.
Path-discovery cases retrieve an intermediate through the public source
callbacks, validate it with the native provider, and reject a tampered
intermediate signature.
When `TINY_CRYPTO_TLV_CORPUS` names the checked-in vector root,
`test_x509_path_corpus` runs seven pinned C2SP x509-limbo graphs through the
native path builder and nine reviewed external revocation cases. The path slice
covers a resolvable cross-sign cycle, an alternate path around an expired dead
end, distinct-CA and same-logical-CA cycles, a wrong issuer key, bounded
same-name/distinct-key expansion, and an alternate constrained intermediate.
The revocation slice covers the limbo direct-anchor revoked case and NIST PKITS
ValidCertificatePathTest1 with the Trust Anchor Root and Good CA complete CRLs.
It also checks CRL signer key usage, absent and issuer-specific serial numbers,
and missing or critical CRL numbers.
The adapter verifies the frozen limbo corpus hash, the selected PKITS file
hashes, and each explicit outcome before it invokes the bounded munit reader.
Encoded trust-anchor certificates are included in the revocation signer source
so signer certificate constraints can be checked.
`test_x509_openssl` also restricts discovery to one source anchor. It checks
that another anchor with the same name cannot be used as a fallback, that
the selected anchor's name constraints apply, and that invalid indices leave
the source view and results unchanged.
Shared-budget cases cover successful and failed searches, exact and short
budgets, and repeated calls after exhaustion. A callback returning the work
counter as certificate storage must be rejected.

With OpenSSL 3 development files installed:

```sh
make test TINY_CRYPTO_TEST_OPENSSL=ON
make test-sanitize TINY_CRYPTO_TEST_OPENSSL=ON
```

The test provider checks P-256 and P-384 certificate signatures with SHA-256
and SHA-384, plus wrong keys and altered signed bytes. This exercises the
library's provider interface; it does not provide a native signature verifier.
Signed-chain tests compare issuer, validity, CA, path-length, DNS constraint and
policy verdicts with OpenSSL. Policy cases cover required policies, mappings,
mapping inhibition and `anyPolicy`. Each chain has a trust anchor, two
intermediate CAs and a target certificate. Revocation is not exercised here.
If CMake cannot find OpenSSL, pass `CMAKE_ARGS=-DOPENSSL_ROOT_DIR=/path/to/openssl`.

This option also enables `test_arithmetic_openssl_0` and
`test_arithmetic_openssl_1`. They compare Montgomery setup and exponentiation
against OpenSSL BIGNUM for deterministic 1024-, 2048-, and 3072-bit odd moduli.
Both limb widths test exponent 65537. Additional cases use full-width exponents
with 32-bit limbs and 128-bit exponents with byte limbs. They also check the
internal PKCS#1 v1.5 verifier against SHA-1/224/256/384/512 signatures made by
OpenSSL at all three key sizes, including altered digests/signatures, short
signatures and work limits. They do not test library key generation or
private-key protection.

### Unicode normalization

The X.509 normalizer uses Unicode 3.2, as required by RFC 4518. Download its
normalization tests and source tables to run the full Unicode suite:

```sh
unicode_dir=$(mktemp -d /tmp/tiny-crypto-unicode.XXXXXX)
for name in UnicodeData CompositionExclusions NormalizationTest CaseFolding; do
  curl --fail --location \
    "https://www.unicode.org/Public/3.2-Update/$name-3.2.0.txt" \
    -o "$unicode_dir/$name-3.2.0.txt"
done
curl --fail --location https://www.rfc-editor.org/rfc/rfc3454.txt \
  -o "$unicode_dir/rfc3454.txt"
curl --fail --location https://www.rfc-editor.org/rfc/rfc4518.txt \
  -o "$unicode_dir/rfc4518.txt"
make test TINY_CRYPTO_TEST_UNICODE_DIR="$unicode_dir"
make test-sanitize TINY_CRYPTO_TEST_UNICODE_DIR="$unicode_dir"
```

These tests check the generated tables, all five columns of the Unicode
normalization corpus, and name preparation against an oracle using the RFC
tables and Python's Unicode 3.2 normalizer. Reference hashes are checked first.
To regenerate the product tables:

```sh
python3 tools/unicode_tables.py --data-dir "$unicode_dir" \
  --stringprep "$unicode_dir/rfc3454.txt" \
  --license LICENSES/Unicode-3.0.txt --output src/unicode_data.inc
```

### Cryptographic vectors

Keep downloads outside the repository. These adapters check the archive
digests, so use the pinned Wycheproof revision rather than its main branch.

```sh
vector_dir=$(mktemp -d /tmp/tiny-crypto-vectors.XXXXXX)
curl --fail --location \
  https://codeload.github.com/C2SP/wycheproof/zip/3fa63dd0344abb611f1fb1d77e119938603ea230 \
  -o "$vector_dir/wycheproof.zip"
curl --fail --location \
  https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/documents/components/ecccdhtestvectors.zip \
  -o "$vector_dir/ecccdh.zip"
curl --fail --location \
  https://raw.githubusercontent.com/Mbed-TLS/mbedtls/091fd1b18806098d74bd58ace7aacb7f171bcb42/tests/suites/test_suite_asn1parse.data \
  -o "$vector_dir/asn1parse.data"
```

Set the two corpus paths to your local copies. The parser corpus root contains
`piv/`, `x509/`, and optionally `eac/cvc/`. The SM adapter reads the original
`sm_vci_vectors` format and the event-based `nist_sd_33_vectors` and
`nist_sd_33_vectors_v2` formats. Run each capture directory separately.

```sh
cmake -S . -B /tmp/tiny-crypto-full \
  -DCMAKE_BUILD_TYPE=Debug \
  -DTINY_CRYPTO_TEST_FULL=ON \
  -DTINY_CRYPTO_TEST_OPENSSL=ON \
  -DTINY_CRYPTO_SANITIZE=address,undefined \
  -DTINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE="$vector_dir/wycheproof.zip" \
  -DTINY_CRYPTO_TEST_EC_CAVP_ARCHIVE="$vector_dir/ecccdh.zip" \
  -DTINY_CRYPTO_TLV_MBEDTLS_SUITE="$vector_dir/asn1parse.data" \
  -DTINY_CRYPTO_TLV_CORPUS="/absolute/path/to/parser-corpus" \
  -DTINY_CRYPTO_TEST_SM_CAPTURE_DIR="/absolute/path/to/sm_vci_vectors"
cmake --build /tmp/tiny-crypto-full --parallel
ctest --test-dir /tmp/tiny-crypto-full --show-only
ctest --test-dir /tmp/tiny-crypto-full --output-on-failure
```

Check the test listing before treating this as a full run. Expect
`test_wycheproof_ec`, `test_wycheproof_kmac`, `test_wycheproof_dynamic_cmac`,
`test_wycheproof_hmac`, `test_wycheproof_aead`, `test_ec_cavp`, `test_sm_primitives_corpus`,
`test_tlv_external_lengths`, and the parser corpus tests. Optional tests can
be absent when their paths are unset or Python is unavailable.
Also check for `test_wycheproof_ecdsa`, `test_wycheproof_rsa_signatures`,
`test_wycheproof_rsa_oaep`, `test_cms_native`, and the OpenSSL RSA private-operation
tests. On macOS, this configuration includes `test_twic_authenticate_command`.

To repeat a configured suite, including its external vectors:

```sh
cmake --build /tmp/tiny-crypto-full --parallel
ctest --test-dir /tmp/tiny-crypto-full --output-on-failure
```

After a failure, use `ctest --test-dir /tmp/tiny-crypto-full --rerun-failed
--output-on-failure` to retry just the failed tests. CTest keeps detailed output
in `/tmp/tiny-crypto-full/Testing/Temporary/LastTest.log`. Run the whole suite
again after fixing the failure.

The external Wycheproof adapter currently covers P-256 and P-384 ECDH, with
raw points and DER public keys, on both EC arithmetic implementations.
Valid cases must produce the expected secret; invalid cases must be rejected.
Its acceptable cases are rejected under the API's strict DER, named-curve,
uncompressed-point policy. This is not yet coverage of every applicable
Wycheproof algorithm.

KMAC256 runs the no-customization suite in strict and relaxed builds. Valid
tags must match the digest; invalid tags must differ. These are digest
comparisons, not tests of a tag-verification API.

Dynamic AES-CMAC runs all three key sizes, checks invalid-key rejection, and
compares valid and invalid tags. HMAC runs SHA-1/224/256/384/512 through the
same tests used for the checked-in vectors. Short HMAC tags are compared with
the full digest, but the verification API must reject tags below its configured
minimum length. Longer valid tags must verify; altered tags must fail.

AES-GCM, CCM, GMAC, EAX, and both AES-SIV formats run at all three AES key
sizes. Valid cases check
encryption and decryption. Invalid cases must fail without changing a separate
output buffer. GCM and CCM authentication mismatches must wipe an in-place
buffer; EAX must leave its ciphertext unchanged.
SIV follows its separate API contract: an authentication mismatch wipes the
output, whether separate or in-place. The two SIV formats differ in their
associated-data components and whether the synthetic IV prefixes the ciphertext.

The pinned archive also contains algorithms and formats outside this API:
PKCS#5-padded CBC, AES key wrap, XTS, FF1, GCM-SIV, chunked encryption, HKDF,
PBKDF2, KMAC128, SHA-3 HMAC, and signature operations. Those are not counted.
Raw CBC has no padding-validation API to test against PKCS#5 rejection cases.
ECDH PEM and WebCrypto import formats are also outside the API; the raw-point
and DER suites exercise the supported key inputs.

Capture replay checks key derivation, handshake messages, protected commands,
responses, and session state. Parser corpus tests check structure and fields,
not certificate signatures or trust.

Some event-based captures retain derivation data for only their last session.
The adapter fails if an earlier session lacks that data. To inspect the
available sessions, run it directly with `--allow-incomplete`:

```sh
python3 tests/piv/sm_corpus.py \
  --reader /tmp/tiny-crypto-full/test_sskdf \
  --sm-reader /tmp/tiny-crypto-full/test_piv_sm \
  --corpus /absolute/path/to/nist_sd_33_vectors_v2 \
  --allow-incomplete
```

This prints the missing session IDs. A partial replay does not count as a
passing full capture suite; CTest does not enable this option.

`test_piv_sm_synthetic` and its CS2-only and CS7-only variants run generated
sessions without external captures. They check the handshake and a protected
exchange, including the malformed-handshake cases in the C replay test.

For an independent EC comparison, install Python's `cryptography` package in
a virtual environment and configure with `TINY_CRYPTO_TEST_EC_ORACLE=ON` and
`Python3_EXECUTABLE` pointing to that environment's Python. This adds
`test_ec_oracle`; the NIST archive test does not need that package.
With the OpenSSL host tests available, it also adds `test_cms_command`, which
generates certificates, SignedData and CRLs using Python `cryptography` and
runs the CMS command-line example. It covers valid and revoked paths, expired
certificates, altered signatures, malformed files and S/MIME capabilities.

```sh
ctest --test-dir build -R '^test_cms_command$' --output-on-failure
```

## Compilers and profiles

```sh
make test-compilers TINY_CRYPTO_TEST_FULL=ON \
  TOOLCHAINS='gcc-15:g++-15 clang:clang++'

for profile in micro mini desktop; do
  make test BUILD_DIR="/tmp/tiny-crypto-$profile" \
    TINY_CRYPTO_RESOURCE_PROFILE="$profile" TINY_CRYPTO_TEST_FULL=ON
done
```

Unavailable compiler pairs are reported and skipped. At least one pair must
run, so check the output if you need coverage from both GCC and Clang.
Profile tests exercise the configured library alongside the suite's dedicated
algorithm configurations.

`test_piv_targets` builds both `piv-acu` and `piv-pd` across all three resource
profiles, checks their enabled algorithms, and rejects a missing required
feature. Each combination also links and runs a munit consumer that accepts a
valid GZIP member and rejects a corrupted checksum. It also verifies a synthetic
P-256/P-384 signature through the native X.509 provider, rejects a changed digest,
and checks exhausted verification budgets.
RSA checks use frozen synthetic 1024/2048/3072-bit fixtures for v1.5 and PSS,
including changed digests and exhausted budgets. Their test-only generator uses
Python cryptography to generate and verify signatures; private keys stay in memory.
A SHA-1 known-answer check covers the hash required by legacy TWIC credentials.
The [ESP32-P4 target](esp32-p4.md)
adds ESP-IDF cross-builds for both roles.

On Linux with Clang, `make test-msan CC=clang CXX=clang++` enables
MemorySanitizer for the core suite. Use `make test-msan-full CC=clang
CXX=clang++ TINY_CRYPTO_TEST_FULL=ON` for extended tests. It needs a compatible
instrumented runtime; Apple Clang does not provide it.

## PIV CVC verification

With `TINY_CRYPTO_TEST_OPENSSL=ON`, `test_piv_cvc_verify` generates independent
CS2/CS7 card CVCs with direct ECDSA signers and RSA-signed intermediates. It checks
issuer/SKI and UUID binding, intermediate subject derivation, malformed points,
altered signatures, role order, missing providers and work-limit boundaries.
The composed CVC helper also checks signer trust, revocation, time, content-signing
policy, key usage, unavailable snapshots and explicit TWIC policy selection.
Run it with `ctest --test-dir build -R '^test_piv_cvc_verify$' --output-on-failure`.

`test_piv_sm_authenticate` checks direct and intermediate CS2/CS7 chains through
key confirmation and protected command/response traffic. Its negative cases
cover altered signatures, UUIDs, cryptograms, transport status, and work limits.

With `TINY_CRYPTO_TLV_CORPUS` set, `test_piv_cvc_corpus` checks 19 captured CVC
file instances. Three direct chains verify. Three intermediate chains have
valid signatures but fail the required public-key subject binding. Captures
without matching signers receive parsing checks only.

```sh
cmake --build build --target test_piv_cvc_corpus_reader test_piv_sm_authenticate
ctest --test-dir build -R '^test_piv_(cvc_corpus|sm_authenticate)$' --output-on-failure
```

## Fuzzing

Fuzzing runs separately from CTest. Use Clang with libFuzzer support and keep
the writable corpus and crash artifacts outside the source tree.
CI builds all four harnesses and runs each for 60 seconds with an 8192-byte input
limit and a two-second per-input timeout.
On macOS, use Homebrew LLVM if the Apple toolchain lacks the libFuzzer runtime;
set the C and C++ compiler paths to its `clang` and `clang++` executables.
The SM harness tests raw malformed responses and builds authenticated CS2/CS7
responses from fuzz input to exercise decryption, invalid padding, and
output-buffer retries.
The GZIP harness checks decoding with varied capacity and work limits, including
failure wiping, workspace cleanup and output boundaries.
The PKI harness includes all three PIV certificate-container profiles. It checks
that returned spans stay within the input and failures preserve the result.
It also exercises PIV/TWIC certificate identifiers with bounded work and checks
FASC-N and TWIC UUID decode/encode round trips. Seed it with synthetic DER
GeneralNames, packed 25-byte FASC-Ns and 16-byte UUIDs to reach those checks.
Security-object checks cover BA/BB/FE mappings and LDS hash lookup, including
absent groups and output preservation. LDS input can be plain DER or a complete
BER OCTET STRING; fragmented content exercises caller-buffer assembly.

```sh
cmake -S . -B /tmp/tiny-crypto-fuzz \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DTINY_CRYPTO_BUILD_FUZZERS=ON
cmake --build /tmp/tiny-crypto-fuzz --target fuzz_tlv fuzz_pki fuzz_piv_sm fuzz_gzip --parallel
fuzz_dir=$(mktemp -d /tmp/tiny-crypto-fuzz-inputs.XXXXXX)
mkdir "$fuzz_dir/tlv" "$fuzz_dir/pki" "$fuzz_dir/sm" "$fuzz_dir/gzip"
/tmp/tiny-crypto-fuzz/fuzz_tlv "$fuzz_dir/tlv" \
  -artifact_prefix="$fuzz_dir/" -max_total_time=300
/tmp/tiny-crypto-fuzz/fuzz_pki "$fuzz_dir/pki" \
  -artifact_prefix="$fuzz_dir/" -max_total_time=300
/tmp/tiny-crypto-fuzz/fuzz_piv_sm "$fuzz_dir/sm" \
  -artifact_prefix="$fuzz_dir/" -max_total_time=300
/tmp/tiny-crypto-fuzz/fuzz_gzip "$fuzz_dir/gzip" \
  -artifact_prefix="$fuzz_dir/" -max_total_time=300
```

## RSA arithmetic and encoding tests

`test_rsa_oaep_decrypt_0` and `test_rsa_oaep_decrypt_1` check public OAEP
decryption against OpenSSL ciphertexts at each RSA size. Independent `hash` and
`mgf` parameters select SHA-1, SHA-224, SHA-256, SHA-384 or SHA-512. Cases cover
empty, one-byte and maximum messages. Failure cases use the maximum message
length; SHA-256 with MGF1-SHA-256 also checks RNG failure, zero work, zero
ciphertext and the RSA input boundary. The `label` parameter selects empty,
one-byte or 256-byte binary labels. Native limbs cover the complete parameter
matrix. Byte limbs cover every RSA-1024 combination plus the SHA-256
configuration at RSA-2048 and RSA-3072.

With `TINY_CRYPTO_TEST_WYCHEPROOF_ARCHIVE` configured, run
`ctest --test-dir build -R '^test_wycheproof_rsa_oaep$' --output-on-failure`
to check the pinned OAEP corpus with both limb profiles. The runner includes
supported groups from mixed-parameter files and reports excluded parameter
combinations. Coverage uses RSA-1024/2048/3072 and SHA-1/224/256/384/512;
SHA-512/224, SHA-512/256 and other RSA sizes are counted separately.

With OpenSSL tests enabled, `test_cpp_rsa_openssl_0` and
`test_cpp_rsa_openssl_1` exercise the C++ PSS signing/verification and OAEP
decryption wrappers with RSA-1024 keys. They check digest mismatch rejection,
decrypt OpenSSL ciphertexts and check workspace cleanup.

`test_rsa_key_openssl` parses OpenSSL-generated PKCS #1 DER private keys at
1024, 2048 and 3072 bits, including their PKCS #8 containers.
It compares all eight borrowed components with OpenSSL
and runs the key-loading example to validate the key and sign a SHA-256 digest.
The `format` parameter selects `pkcs1`, `pkcs8` or `pkcs8-pss`;
all run by default. PSS uses SHA-256/MGF1-SHA-256 and a 32-byte salt.
PSS-only key containers must reject v1.5 signing before requesting entropy.
The `/rsa/key/restricted-pss` case generates restricted RSA-PSS keys with OpenSSL,
imports their PKCS #8 containers and verifies signatures from the PSS example.
It also checks minimum-salt enforcement, rejection of v1.5 signing and an RNG
failure during salt generation. The fault case checks signature preservation,
used-workspace cleanup and the untouched workspace tail.
OpenSSL verifies the resulting signature. Failure cases cover every truncated
encoding, a changed modulus, insufficient scratch and a failing random source.
They check signature preservation and RNG-failure scratch cleanup.
For a targeted run:

```sh
./build/test_rsa_key_openssl --param bits 1024
./build/test_rsa_key_openssl --param bits 1024 --param format pkcs8
```

`test_rsa_import` checks PKCS #8 key parsing and RSA-PSS usage restrictions with
X.509 and hash implementations disabled. Cases cover truncated containers,
public/private key mismatches, unsupported algorithms and unchanged output on
failure. Run it with `./build/test_rsa_import`.

```sh
./build/test_rsa_oaep_decrypt_0
./build/test_rsa_oaep_decrypt_1 --param bits 1024
./build/test_rsa_oaep_decrypt_0 --param bits 2048 --param hash SHA512 --param label 256
```

When `avr-gcc` is available, CMake adds compile checks for the RSA implementation
and validation/signing examples. They use ATmega2560's 16-bit `size_t` with warnings
treated as errors. The AVR C++ header check also checks the validation API's
32-bit work parameter. These checks require the compiler, with no connected board.

```sh
ctest --test-dir build -R '^test_(rsa(_validate|_sign)?_compile_avr|cpp_headers_avr)$' --output-on-failure
```

`test_rsa_private_openssl_0` and `test_rsa_private_openssl_1` compare the internal
blinded private operation against OpenSSL at 1024, 2048, and 3072 bits. Cases
cover rejected blinding factors, RNG failures, work and storage limits, invalid
private inputs, output preservation, and scratch cleanup. A changed private
exponent exercises the final public-exponent check. Key-consistency cases check
the factor product, exponent congruences, swapped factors, damaged exponents and
factors, invalid factor shapes, exact work limits, and scratch cleanup.
Combined component/primality tests cover both factors, RNG errors, exact and
exhausted work, short scratch, and cleanup. A composite-factor fixture satisfies
the component equations and must fail primality testing. These orchestration
tests use one round per factor; the sampler tests cover multiple rounds.
The public API cases check all 65 rounds per factor at each key size, workspace
sizing, component lengths, metadata overlap, and preflight limits.
The `bits` parameter selects one key size for a focused run:

```sh
./build/test_rsa_private_openssl_0 --param bits 1024
./build/test_rsa_private_openssl_1 --param bits 1024
```

The `/rsa/private/signing` case verifies public v1.5 signatures with OpenSSL and
the library verifier for SHA-1, SHA-224, SHA-256, SHA-384 and SHA-512. The test
libraries have hash implementations disabled. It also checks exact work,
exhausted work, RNG failure,
output preservation and scratch cleanup.

Native limbs run the complete private-operation matrix at RSA-1024, RSA-2048 and
RSA-3072. Byte limbs run that matrix at RSA-1024; OAEP and PSS exercise the same
private arithmetic at the larger sizes. Byte-limb v1.5 signing uses every hash at
RSA-1024 and SHA-256 at RSA-2048 and RSA-3072.

`test_rsa_pss_openssl` and `test_rsa_pss_openssl_small` cross-check PSS signing
and verification with OpenSSL. Native limbs cover every combination of RSA size,
message hash, MGF hash and supported salt length. Byte limbs cover the complete
RSA-1024 matrix plus SHA-256 with MGF1-SHA-256 at RSA-2048 and RSA-3072. The
SHA-256 cases also check oversized salts, RNG failures, zero-work preflight,
exact budgets, output preservation and scratch cleanup.

Use the `bits`, `hash`, and `mgf` parameters to narrow the PSS matrix:

```sh
./build/test_rsa_pss_openssl --param bits 1024
./build/test_rsa_pss_openssl_small --param bits 1024 --param hash SHA512 --param mgf SHA1
```

```sh
./build/test_rsa_private_openssl_0 /rsa/private/signing
./build/test_rsa_private_openssl_1 /rsa/private/signing --param bits 1024
./build/test_rsa_private_openssl_0 /rsa/private/signing --param bits 2048 --param hash SHA384
```

`test_rsa_validation_0` and `test_rsa_validation_1` check public input ranges
without generating keys. They cover missing pointers, overflowing lengths,
component and metadata overlap, short storage, and word alignment. Rejected
calls must preserve scratch and leave the RNG untouched.
The `/rsa/validation/operation-ranges` case adds signature overlap with every key
component, digest, scratch and metadata, plus malformed digest/output lengths,
unknown hashes and exhausted limits. OAEP checks cover plaintext and length-output
overlap with inputs, scratch and metadata, plus missing or misaligned length
pointers. The entire fixture must remain unchanged.

```sh
ctest --test-dir build -R '^test_rsa_validation_[01]$' --output-on-failure
```

The `test_rsa_inverse_0` and `test_rsa_inverse_1` arithmetic tests cover every
input below each odd modulus from 3 through 255, plus carry boundaries and
non-invertible inputs. With OpenSSL enabled, `test_rsa_inverse_openssl_0` and
`test_rsa_inverse_openssl_1` check random odd moduli and selected composite cases
at 1024, 2048, and 3072 bits. The suffix selects 32-bit or 8-bit limbs.

```sh
ctest --test-dir build -R '^test_rsa_inverse(_openssl)?_[01]$' --output-on-failure
```

`test_rsa_prime_0` and `test_rsa_prime_1` check individual Miller-Rabin rounds
against a small-integer reference, including strong pseudoprimes. The OpenSSL
variants compare rounds at 512-, 1024-, and 1536-bit storage widths, covering
limb-boundary shifts, zero-padded primes, and scratch bounds. Sampling tests cover
multiple rounds, rejected bases, RNG failure, exhausted requests and work,
short scratch, and cleanup. Prepared-round tests check that candidate setup
survives both passing and failing bases. The internal sampler takes a
caller-supplied round count and an odd candidate, with an exact check for three.
Padded-candidate tests cover bit-width masking and every sampled byte for small
odd candidates. The OpenSSL
variants also exercise sampling at RSA factor widths, with exact work budgets
and scratch cleanup checks.

```sh
ctest --test-dir build -R '^test_rsa_prime(_openssl)?_[01]$' --output-on-failure
./build/test_rsa_prime_openssl_1 --param bits 512
```

`test_rsa_oaep` checks the internal OAEP encoder and decoder, independently of
RSA arithmetic. It covers a SHA-256 known answer, every message length for
128-, 256-, and 384-byte encodings with supported SHA hashes and MGF1-SHA-256,
oversized messages, malformed padding, and exact/short work budgets. Failed
decodes must leave the message view unchanged. These tests do not establish
private-key operation or timing resistance.

`test_rsa_oaep_arguments` checks algorithm selection, missing storage, seed width,
length overflow, and the SHA-256 label limit. `test_rsa_oaep_disabled` uses the
same cases with hashes disabled, checking that unsupported algorithms preserve
the encoded buffer, message view, and work budget. Hash-dependent limit cases
are skipped in that profile.

With OpenSSL tests enabled, `test_rsa_oaep_openssl` and its `_small` variant check
encoding, decoding and public encryption using 1024-, 2048-, and 3072-bit keys.
They cover every
fitting SHA-1/SHA-2 hash and MGF1 combination, empty and binary labels, empty,
one-byte and maximum-length messages, and wrong-label rejection. Public encryption
is compared byte-for-byte with an independently transformed OAEP block using the
same seed, then decrypted by OpenSSL. Failure cases check output preservation,
scratch cleanup, RNG failure, short buffers, overlap and work limits.

```sh
ctest --test-dir build -R '^test_rsa_(oaep.*|pss|mgf)$' --output-on-failure
./build/test_rsa_oaep_openssl --param bits 2048
```

## Signature corpus tests

`test_wycheproof_ecdsa` reads every fixed-width and DER ECDSA dataset for
implemented P-192, P-256 and P-384 curves from the pinned archive. Both limb
widths receive positive and negative cases. Other curves are counted separately.
The host computes SHA-2, SHA-3 and SHAKE digests; the C reader decodes DER
signatures with `TC_DER_ecdsa_signature` when needed, then calls
`TC_ECDSA_verify_digest`. SHAKE128 and SHAKE256 use 32-byte and 64-byte outputs
as specified by [RFC 8692 section 3.2](https://www.rfc-editor.org/rfc/rfc8692.html#section-3.2).
This checks prehashed signature verification; CMS algorithm selection and hash
implementation availability have separate tests.

```sh
ctest --test-dir build -R '^test_wycheproof_ecdsa$' --output-on-failure
```

`test_wycheproof_rsa_signatures` runs supported groups from every pinned
RSA-PSS and PKCS #1 v1.5 signature file on both limb widths. It covers enabled
SHA-1/SHA-2 digests, distinct MGF1 hashes and explicit salt lengths. Missing-NULL
DigestInfo cases use strict rejection. Unsupported hashes and key sizes are
counted separately. Parameters are passed directly to the digest verification
APIs; DER-encoded PSS parameter parsing has separate CMS and key-import tests.

```sh
ctest --test-dir build -R '^test_wycheproof_rsa_signatures$' --output-on-failure
```

With the OpenSSL test option enabled, `test_rsa_pss_openssl` and
`test_rsa_pss_openssl_small` check RSA-1024/2048/3072 against generated
OpenSSL signatures. Both limb widths exercise PSS and the internal PKI
verification dispatch, including v1.5 signatures and negative cases.

## ESP-IDF signed-image tests

The host image tests accept fixtures signed with Espressif's `espsecure`
tool. Use disposable test keys, never production keys. With the IDF Python
environment active, create a fixture from an application binary:

```sh
python -m espsecure generate_signing_key --version 2 --scheme ecdsa256 test-key.pem
python -m espsecure sign_data --version 2 --keyfile test-key.pem --output signed.bin application.bin
python -m espsecure verify_signature --version 2 --keyfile test-key.pem signed.bin
```

Repeat with `ecdsa192` and `rsa3072`, using separate key and output paths.
Register the three signed images when configuring the host tests:

```sh
cmake -S . -B build \
  -DTINY_CRYPTO_TEST_ESP_SIGNED_IMAGE=/path/to/rsa3072.bin \
  -DTINY_CRYPTO_TEST_ESP_ECDSA192_IMAGE=/path/to/ecdsa192.bin \
  -DTINY_CRYPTO_TEST_ESP_ECDSA256_IMAGE=/path/to/ecdsa256.bin
cmake --build build --parallel
ctest --test-dir build -R '^test_idf_(ecdsa192_image|ecdsa256_image|signed_rsa_image)$' --output-on-failure
```

These tests recompute the image digest and exercise the application crypto
adapters with valid signatures, changed digests, changed signatures and
invalid key parameters. They do not exercise IDF's trusted-key selection,
signature-block CRC checks, anti-rollback or flash writes. Run the same tests
in a sanitizer build to check memory access and undefined behavior.

The `test_idf_signature_policy_*` targets compile the vendored IDF verifier
for RSA and ECDSA, with eFuse and single-signature configurations. These check
eFuse and running-image trust selection, signature-block CRC and scheme,
missing keys, explicit digest revocation, read failures and crypto error
propagation. Crypto results are stubbed in these policy tests; use the image
tests above for the actual signature calculations. Run both with:

```sh
ctest --test-dir build -R '^test_idf_signature_policy_' --output-on-failure
```

When signed-image paths are configured, combined tests run the same policy
code with the real hash and signature adapters. They cover eFuse and
running-image trust, bad CRC, changed digests, unknown keys and altered
signatures with recomputed CRCs:

```sh
ctest --test-dir build -R '^test_idf_(rsa|ecdsa192|ecdsa256)_image_policy$' --output-on-failure
```

## Benchmark checks

```sh
make benchmark
make benchmark-report-check
```

The first command measures host throughput. The second checks the generated
Uno and RP2350 resource report and needs their cross-compilers. See
[benchmarks](benchmarks.md) for the measured configurations. Neither replaces
the correctness suites above.
