<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# X.509 path validation

Include `<tiny_crypto/x509_path.h>` and enable `TINY_CRYPTO_ENABLE_X509_PATH`.
`TC_X509_path_validate` checks an ordered certificate path against an explicit
trust anchor. Path discovery and revocation checking are separate steps.
The [CRL reader](x509-crl.md) provides separate parsing of revocation lists.
Use [path revocation checking](x509-revocation.md) after validation.
The [trust-anchor reader](x509-trust-anchors.md) supplies RFC 5914 anchors
whose RFC 5937 constraints are enforced for the selected path.
`TC_X509_store_anchor_from_certificate` builds the same record from one
parsed root certificate for `TC_X509_path_validate_with_anchor`.

Signature verification comes from `options.signatures`. Use
[`TC_X509_native_provider`](x509-crypto.md) for the library's ECDSA and RSA
v1.5/PSS implementations, or supply a hardware provider. The caller selects the
provider explicitly. The OpenSSL provider used by the tests stays in the test tree.

The verifier callback receives an ordered array of message spans and a count.
Hash or consume each span in order. The spans hold message bytes for the
provider to hash. Certificate verification supplies one span containing the
original DER TBSCertificate. `TC_X509_signature_verify_message` also accepts segmented messages
without combining them in a temporary buffer. Empty spans are allowed. The
callback must check algorithm parameters and key compatibility and charge its
work against the supplied budget. All message storage remains caller-owned.

## Inputs

Pass an array of DER spans, starting with the certificate issued by the trust
anchor and ending with the target. Leave the anchor certificate out of that
array. The anchor supplies the trusted name and public key. A self-signature
carries no trust.

Initialize `TC_X509_path_options` to zero, then set:

- `at`: the validation time in UTC, supplied by the caller's clock.
- `clock_skew_seconds`: tolerance for clock differences. Each certificate's
  validity period is widened by this amount on both sides. Zero requires
  `notBefore <= at <= notAfter` exactly. `TC_X509_time_check` validates a
  caller-built time.
- `parsing`: the input, value, element and nesting limits for a certificate.
- `max_certificates`, `max_input`: the chain length and total DER-byte limits.
- `max_work`: a shared processing budget, including the signature provider.
- `signatures`: the verifier callback and its context.

The optional `purpose` contains an EKU OID's DER contents, without its tag and
length. Set `key_usage` with the `TC_KEY_USAGE_*` masks, for example
`TC_KEY_USAGE_DIGITAL_SIGNATURE` for a signing key.
Present EKU extensions constrain the requested purpose throughout the path.
The requested key-usage bits apply to the target. The `REQUIRE_KEY_USAGE` and
`REQUIRE_EXTENDED_KEY_USAGE` flags also require those extensions on the target.
Set `TC_X509_PATH_INHIBIT_ANY_PURPOSE` to require the requested OID explicitly
when an EKU extension is present. This disables the `anyExtendedKeyUsage`
wildcard throughout the path. Combine it with `REQUIRE_EXTENDED_KEY_USAGE` when
the target must carry EKU. Certificate-profile rules such as criticality and
exclusive use of one EKU require separate profile checks.

`initial_policies` contains the acceptable policy OIDs, also as DER contents.
An empty list is the RFC 5280 default user-initial-policy-set, `{anyPolicy}`,
and accepts the authority-constrained policy set. Listing `anyPolicy`
(`55 1d 20 00`) has the same effect. Set `TC_X509_PATH_REQUIRE_EXPLICIT_POLICY`
when a nonempty selected set is required.
The mapping and any-policy inhibition flags apply independently.

`anchor_names` supplies additional permitted/excluded subtrees from trusted
configuration. These are GeneralSubtree list contents, as returned by
`TC_X509_name_constraints_read`. Self-issued intermediates receive the standard
exception. The target receives none.

## Workspace and buffer lifetime

The validator needs twelve caller-owned scratch arrays. Most applications
describe their element counts in a `TC_X509_path_capacity` and let the library
partition one arena:

```c
#include <tiny_crypto/x509_path.h>

static TC_X509_path_storage arena[1024];

TC_result prepare_path_workspace(TC_X509_path_workspace* workspace)
{
    static const TC_X509_path_capacity capacity = {
        .frames = 16, .oids = 16, .name_scalars = 64, .name_attributes = 8,
        .policy_nodes = 32, .policy_edges = 64, .policy_expected = 64,
        .policy_mappings = 16, .policies = 16, .path = 4};
    const TC_buffer storage = {(uint8_t*)arena, sizeof arena};
    size_t required;
    TC_result status = TC_X509_path_workspace_size(&capacity, &required);
    if (status != TC_RESULT_OK)
        return status;
    if (required > sizeof arena)
        return TC_RESULT_LIMIT;
    return TC_X509_path_workspace_init(&capacity, storage, workspace);
}
```

`TC_X509_path_workspace_size` reports the exact arena bytes, including the
padding that aligns each array. `TC_X509_path_workspace_alignment` reports the
required base alignment, and an array of `TC_X509_path_storage` meets it.
`TC_X509_path_workspace_init` rejects a misaligned base with
`TC_RESULT_ARGUMENT` and a short arena with `TC_RESULT_LIMIT`. Both failures
leave the workspace unchanged. Initialization writes only the workspace
descriptor, and the arena contents stay untouched until validation.
`capacity.path` sizes the certificate views and extension summaries, one
entry per path certificate. `name_scalars` sizes both name buffers. Zero policy
counts leave those arrays NULL. Validation returns `TC_X509_PATH_LIMIT` when a
policy array runs out. The policy tree always holds its anyPolicy root, so
`policy_nodes` needs at least one entry.

The workspace borrows the arena. Keep the arena alive while the workspace or a
result that borrows it is in use, and give each concurrent validation its own
arena.

For fixed array locations, set each field of `TC_X509_path_workspace` directly
with capacities as element counts. `TC_X509_PATH_WORKSPACE_INIT` fills the
workspace from arrays and infers their capacities. Pass arrays, including array
members of a storage structure. Pointers lose the array size. The smaller of the
two name arrays sets the scalar capacity. [examples/x509_workspace.h](../examples/x509_workspace.h)
uses this form.

The certificate array needs at least one entry per path certificate. A
shorter array returns `TC_X509_PATH_LIMIT`. The node, edge and expected-policy
arrays belong to the validator. Treat their working fields as private.

One OID array is reused for extension decoding, policy decoding and EKU checks.
The returned policy array is separate. The two name buffers hold prepared
Unicode scalars, and the attribute buffer holds matching state for one RDN.

The basic pass parses each certificate once into the caller-owned certificate
array. Later passes reuse those views. Each view borrows its DER, key bytes and
extension values from the original buffers. Workspace limits bound the policy
graph. The work budget also bounds scanning and comparison work.

The summary array holds one `TC_X509_extension_summary` per path
certificate. Each certificate's extensions are scanned once per validation,
and the subject and issuer of each intermediate are compared once. Every pass
reads the summary. A NULL or shorter summary array returns
`TC_X509_PATH_LIMIT`. The validator owns the summary fields. Size the array
like the certificate array.

Workspace arrays must not overlap each other, the inputs or the result object.
The signature provider's context must also be separate from workspace and the
result. Input spans must remain valid and unchanged throughout validation.
Certificate views are scratch and may be overwritten by the next validation.

After success, the returned key borrows the target DER. Policy OIDs can borrow
issuer DER or `initial_policies`, and the policy-span array belongs to workspace.
Keep those buffers alive until the result is no longer needed. Reusing the
workspace can overwrite the returned policy array.

Path discovery also returns its path-span array in search workspace. Before
reusing either workspace, copy any path and policy spans you still need into
caller-owned arrays. Copying only `TC_X509_search_report` leaves its pointers in
the original scratch arrays. The certificate DER stays in its shared buffers.
Keep those buffers and the source snapshot alive.

## Client-certificate example

[examples/x509_client.c](../examples/x509_client.c) configures a client-authentication
purpose and digital-signature key usage. It lays out its validation workspace
in a caller-supplied arena with `TC_X509_path_workspace_init`. The example
accepts up to four certificates of 4096 bytes each. Its graph and name
capacities are explicit in its `TC_X509_path_capacity`. Adjust those limits to
the certificates and memory available in your application.

Compile the example source with your application and link `tiny_crypto_c`. Supply
the trusted anchor, UTC validation time, signature provider and work budget to
`example_check_client_certificate`. Check the arena once at startup against
`example_client_workspace_size`. The example returns the same statuses as the
library API and leaves the result unchanged on failure. A short arena returns
`TC_X509_PATH_LIMIT`, and a NULL or misaligned arena returns
`TC_X509_PATH_ERROR`. Consume the result only after
`TC_X509_PATH_VALID`. Handle unsupported operations and limit failures separately
from invalid certificates.

Place the arena in application-owned storage outside a small task stack. Use a
separate arena for concurrent validations. Keep it
and the original certificate buffers alive while using the result. Revocation
evidence must be checked separately before granting application access.

The installed-consumer test copies this example outside the source tree and
builds C99 and C++11 callers against an installed library. The OpenSSL suite
also runs valid, invalid and work-limited signed-chain cases through it.

## Results

- `TC_X509_PATH_VALID`: the ordered-path checks succeeded. `out` is populated.
- `TC_X509_PATH_INVALID`: the certificate encoding or path requirements failed.
- `TC_X509_PATH_UNSUPPORTED`: a required algorithm, name rule or critical extension
  is unavailable in the configured build.
- `TC_X509_PATH_LIMIT`: an input, workspace or processing limit was reached.
- `TC_X509_PATH_ERROR`: invalid arguments or options, overlapping storage, a
  provider error or a storage source that failed to supply bytes. Invalid
  options, such as `options.at` or a malformed initial-policy OID, are
  reported before any work is charged.

Only `VALID` authorizes use of the returned key under the supplied path settings.
Other statuses leave `out` unchanged. Workspace and provider state may change
on any result. Revocation status requires a separate check.

## Verification

The signed-chain tests exercise the public API against OpenSSL verdicts, including
invalid signatures, validity periods, CA constraints, name constraints, policies,
usage restrictions and unknown critical extensions. See [Testing](testing.md)
for the OpenSSL test configuration and full-suite commands.
