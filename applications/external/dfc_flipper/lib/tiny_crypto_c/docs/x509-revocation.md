<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# X.509 path revocation

Include `<tiny_crypto/x509_revocation.h>` and enable
`TINY_CRYPTO_ENABLE_X509_REVOCATION`.
`TC_X509_path_check_revocation` checks CRLs for a previously validated certificate
path. It does not replace path validation or fetch certificates and CRLs.
The path operation, candidate-source guards, storage preflight and dependency
resolution are implemented by the X.509 revocation layer. CMS validation uses
the same operation with an adapter for embedded certificate collections.

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
- `signer_policy`: validation options for CRL signers at the same time. Do not
  reuse a holder-specific EKU or key-usage requirement; cRLSign is added internally.
- `max_candidate_bytes`: the total encoded-byte limit for the candidate collection.

`delta_policy` selects complete CRLs only, deltas when available, or required
deltas. `order_policy` normally uses CRL numbers. `TC_X509_CRL_ORDER_THIS_UPDATE`
explicitly enables time ordering for legacy unnumbered CRLs; there is no automatic
fallback, and delta pairing still requires numbers.

## Workspace and results

`TC_X509_revocation_workspace` borrows validation and search workspaces. Its
`states` array needs one byte per indexed CRL. Its node array must cover all
distinct path certificates and signer dependencies visited during the call.
Insufficient storage or work returns `TC_TLV_LIMIT`.

Verified dependencies are shared across path members within one call. They are
not retained as trusted results across calls. Cycles without independent evidence
and missing CRLs return `TC_TLV_UNSUPPORTED`. Input bytes, options, source records
and workspace metadata must remain stable while the call runs.

`TC_TLV_OK` means a decision is available, not that authorization succeeded.
Check `result.status`:

- `TC_X509_CRL_UNREVOKED`: every path member has complete reason coverage.
- `TC_X509_CRL_REVOKED`: `certificate_index` identifies the member and `evidence`
  contains its revocation reason and date. Read the invalidity date only when
  `has_invalidity_date` is set.

Other return codes leave the result unchanged. Work, signature states and node
storage are provisional and may change. An unrevoked result uses `SIZE_MAX` for
the member index and zero evidence.

## Example

[examples/x509_revocation.c](../examples/x509_revocation.c) sets up the typed
workspaces from [caller-owned storage](../examples/x509_revocation.h). It supports
four indexed CRLs and eight dependency nodes. The native tests compile and run
the example with unrevoked and revoked issuer paths.

After configuring `options` and preserving `held_path`:

```c
TC_X509_revocation_result result;
TC_TLV_result status = example_check_path_revocation(
    held_path, path_count, &options, &work, storage, &result);
int accepted = status == TC_TLV_OK && result.status == TC_X509_CRL_UNREVOKED;
```

Keep the workspace outside a small task stack. Calls sharing its scratch must
be serialized. The result copies dates and status, but the path, source and CRL
index retain their original buffer lifetimes.
