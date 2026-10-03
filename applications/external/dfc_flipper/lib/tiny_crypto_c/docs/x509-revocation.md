<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# X.509 path revocation

Include `<tiny_crypto/x509_revocation.h>` and enable
`TINY_CRYPTO_ENABLE_X509_REVOCATION`.
`TC_X509_path_check_revocation` checks CRLs and optional OCSP responses for a
previously validated certificate path. Path validation and certificate, CRL and
OCSP retrieval stay with the caller.
The path operation, candidate-source guards, storage preflight and dependency
resolution are implemented by the X.509 revocation layer. One argument check
and one storage preflight cover every path member and dependency node. CMS
validation uses the same operation with an adapter for embedded certificate
collections.

First validate or discover the path. Keep its anchor, validation time and
[trust-store snapshot](x509-store.md) fixed for revocation checking. Pass the
certificates anchor-issued first and target last, without the anchor certificate.
Copy the path-span array out of search scratch before reusing that workspace.
The DER buffers themselves stay shared.

## Configuration

Build a [CRL index](x509-crl.md#indexing-a-collection) and set
`TC_X509_revocation_options`:

- `index`: the parsed CRL collection, kept unchanged throughout the call.
- `source`: the held source with CRL signer certificates, intermediates and anchors.
- `anchor_index`: the same source anchor used to validate the target path.
- `signer_policy`: validation options for CRL signers at the same time. Omit
  holder-specific EKU and key-usage requirements. cRLSign is added internally.
  Version 3 CRL signers must carry keyUsage with cRLSign set.
- `max_candidate_bytes`: the total encoded-byte limit for the candidate collection.
- `time`: the evaluation time and freshness limits for CRLs and OCSP responses.
- `ocsp`: optional OCSP responses, one per path member (see below).

`delta_policy` selects complete CRLs only, deltas when available, or required
deltas. `order_policy` normally uses CRL numbers. `TC_X509_CRL_ORDER_THIS_UPDATE`
explicitly enables time ordering for legacy unnumbered CRLs. It is never selected
automatically, and delta pairing still requires numbers.

## Freshness

`TC_X509_revocation_time` holds `at`, `clock_skew_seconds` and
`max_age_seconds`. CRLs and OCSP responses use one rule. Evidence is current
when thisUpdate is at most `at + clock_skew_seconds`, a present nextUpdate is
later than `at - clock_skew_seconds`, and, when `max_age_seconds` is nonzero,
thisUpdate is at most `max_age_seconds` before `at - clock_skew_seconds`. A CRL
without nextUpdate is never current (RFC 5280 section 6.3.3). An OCSP response
without nextUpdate is current only under a nonzero `max_age_seconds`. CRL
signer paths are validated with `signer_policy`, whose `at` must equal
`time.at`. Its clock skew applies to the signer certificates.

## OCSP evidence

Set `ocsp.responses` to an array with one DER OCSPResponse span per path
member, in chain order, and `ocsp.count` to the path length. An empty span means
no response for that member. `max_responses` and `max_certificates` bound each
response as in `TC_X509_ocsp_verify_request`. Leave `ocsp` zeroed to use CRLs
only. [X.509 OCSP](x509-ocsp.md) describes response verification and responder
authorization.

Each response is verified with `TC_X509_ocsp_response_verify` against the
member's issuer: the selected anchor for the first member and the previous
member otherwise. Delegate candidates come from `source`, after the certs in
the response. The composed check verifies responses without a nonce. An
accepted REVOKED response settles the member. After an accepted GOOD response
the member's CRLs are also consulted, and a CRL that lists the member revoked
takes precedence. A GOOD response signed by a delegate without
`id-pkix-ocsp-nocheck` is accepted only when the CRL index proves the delegate
unrevoked (RFC 6960 section 4.2.2.2.1). The delegate then takes one dependency
node. A REVOKED response from such a delegate is accepted unless the CRL index
shows the delegate revoked. A member whose response is missing, malformed,
unauthorized, stale, UNKNOWN or unavailable, or whose delegate lacks that
proof, falls back to CRLs. Exhausted limits and argument errors
stop the call. Responses must stay unchanged during the call and must not
overlap any workspace array. A build without `TINY_CRYPTO_ENABLE_X509_OCSP`
uses CRLs for every member.

## Workspace and results

`TC_X509_revocation_workspace` borrows validation and search workspaces. Its
`states` array needs one byte per indexed CRL. `scopes` needs one slot per
indexed CRL. Its node array must cover all distinct path certificates and signer
dependencies visited during the call. Each node carries two `size_t` lookup
links, and each scope slot carries three `size_t` links. `signer_path` needs
`search.capacity` spans. `signer_policies` needs
`validation.policy_capacity` spans. The signer arrays hold borrowed views into
the fixed source snapshot. Keep those source bytes unchanged through the call.
Insufficient storage or work returns `TC_TLV_LIMIT`.

Scope groups are reused across point scans for each target. The most recently
validated signer path and its per-CRL signature results are reused when the
same signer appears again. Verified dependencies are shared across path members.
Each call starts with no trusted cached results.
OCSP verification uses the validation workspace.
Cycles without independent evidence and members with no accepted OCSP response
and no CRL evidence return `TC_TLV_UNSUPPORTED`. So does an unsettled member
with an unsupported candidate CRL, such as one with an unknown critical
extension. `TC_TLV_INVALID` means a member has no accepted OCSP response and
its candidate CRLs failed as invalid data, with none unsupported. Causes
include a CRL signature that does not verify, no signer candidate with a valid
path to the anchor, a revoked CRL signer, conflicting CRLs in one scope and
malformed CRL entries. An invalid `time.at`, a `time.at` that differs from
`signer_policy->at`, an `ocsp.count` other than zero or the path length, or
OCSP responses with a zero `ocsp.max_responses` returns `TC_TLV_ARGUMENT`.
Input bytes, options, source records and workspace metadata must remain stable
while the call runs.

`TC_TLV_OK` means a decision is available in `result.status`:

- `TC_X509_REVOCATION_GOOD`: every path member has complete reason coverage.
- `TC_X509_REVOCATION_REVOKED`: `certificate_index` identifies the member and
  `evidence` contains its revocation reason and date. Read the invalidity date
  only when `has_invalidity_date` is set. For OCSP evidence, the date is the
  revocationTime and the reason is the CRLReason, or unspecified (0) when the
  response has none.

Other return codes leave the result unchanged. Work, cache arrays and node
storage are provisional and may change. A GOOD result uses `SIZE_MAX` for the
member index and zero evidence.

## Example

[examples/x509_revocation.c](../examples/x509_revocation.c) sets up the typed
workspaces from [caller-owned storage](../examples/x509_revocation.h). It supports
four indexed CRLs and eight dependency nodes. `test_cms_revocation` compiles
and runs the example with unrevoked and revoked issuer paths.

After configuring `options` and preserving `held_path`:

```c
TC_X509_revocation_report result;
TC_TLV_result status = example_check_path_revocation(
    held_path, path_count, &options, &work, storage, &result);
int accepted = status == TC_TLV_OK && result.status == TC_X509_REVOCATION_GOOD;
```

Keep the workspace outside a small task stack. Calls sharing its scratch must
be serialized. The result copies dates and status, but the path, source and CRL
index retain their original buffer lifetimes.
