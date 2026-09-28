<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# X.509 path validation

Include `<tiny_crypto/x509_path.h>` and enable `TINY_CRYPTO_ENABLE_X509_PATH`.
`TC_X509_path_validate` checks an ordered certificate path against an explicit
trust anchor. It does not find a path or check revocation.
The [CRL reader](x509-crl.md) provides separate parsing of revocation lists.
Use [path revocation checking](x509-revocation.md) after validation.

Signature verification comes from `options.signatures`. Use
[`TC_X509_native_provider`](x509-crypto.md) for the library's ECDSA and RSA
v1.5/PSS implementations, or supply a hardware provider. No provider is selected
implicitly. The OpenSSL provider used by the tests is not part of the library.

The verifier callback receives an ordered array of message spans and a count.
Hash or consume each span in order; these are message bytes, not a precomputed
digest. Certificate verification supplies one span containing the original DER
TBSCertificate. `TC_X509_signature_verify_message` also accepts segmented messages
without combining them in a temporary buffer. Empty spans are allowed. The
callback must check algorithm parameters and key compatibility and charge its
work against the supplied budget. All message storage remains caller-owned.

## Inputs

Pass an array of DER spans, starting with the certificate issued by the trust
anchor and ending with the target. Do not include the anchor certificate in
that array. The anchor supplies a trusted name and public key; a self-signature
does not establish that trust.

Initialize `TC_X509_path_options` to zero, then set:

- `at`: the validation time in UTC. The library does not read a clock.
- `parsing`: the input, value, element and nesting limits for a certificate.
- `max_certificates`, `max_input`: the chain length and total DER-byte limits.
- `max_work`: a shared processing budget, including the signature provider.
- `signatures`: the verifier callback and its context.

The optional `purpose` contains an EKU OID's DER contents, without its tag and
length. Set `key_usage` with the `TC_KEY_USAGE_*` masks, for example
`TC_KEY_USAGE_DIGITAL_SIGNATURE` for a signing key.
Present EKU extensions constrain the requested purpose throughout the path;
the requested key-usage bits apply to the target. The `REQUIRE_KEY_USAGE` and
`REQUIRE_EXTENDED_KEY_USAGE` flags also require those extensions on the target.
Set `TC_X509_PATH_INHIBIT_ANY_PURPOSE` to require the requested OID explicitly
when an EKU extension is present. This disables the `anyExtendedKeyUsage`
wildcard throughout the path. Combine it with `REQUIRE_EXTENDED_KEY_USAGE` when
the target must carry EKU. Certificate-profile rules such as criticality and
exclusive use of one EKU still require profile checks.

`initial_policies` contains the acceptable policy OIDs, also as DER contents.
Use `anyPolicy` (`55 1d 20 00`) to accept the authority-constrained policy set.
An empty initial set produces no selected policies. Set
`TC_X509_PATH_REQUIRE_EXPLICIT_POLICY` when a nonempty selected set is required.
The mapping and any-policy inhibition flags apply independently.

`anchor_names` supplies additional permitted/excluded subtrees from trusted
configuration. These are GeneralSubtree list contents, as returned by
`TC_X509_name_constraints_read`. Self-issued intermediates receive the standard
exception; the target does not.

## Workspace and buffer lifetime

Allocate the typed arrays listed in `TC_X509_path_workspace` and set each
capacity in elements, not bytes. Node, edge and expected-policy storage belongs
to the validator; callers should not interpret or edit its working fields.

`TC_X509_PATH_WORKSPACE_INIT` fills the workspace from arrays and infers their
capacities. Pass arrays, including array members of a storage structure, not
pointers. The smaller of the two name arrays sets the scalar capacity.

One OID array is reused for extension decoding, policy decoding and EKU checks.
The returned policy array is separate. The two name buffers hold prepared
Unicode scalars, and the attribute buffer holds matching state for one RDN.

Certificates are scanned in their original buffers. The validator reuses a
certificate view instead of retaining a parsed structure for every certificate.
It does not copy DER, key bytes or extension values. Workspace limits bound the
policy graph; the work budget also bounds rescanning and comparison work.

Workspace arrays must not overlap each other, the inputs or the result object.
The signature provider's context must also be separate from workspace and the
result. Input spans must remain valid and unchanged throughout validation.

After success, the returned key borrows the target DER. Policy OIDs can borrow
issuer DER or `initial_policies`, and the policy-span array belongs to workspace.
Keep those buffers alive until the result is no longer needed. Reusing the
workspace can overwrite the returned policy array.

Path discovery also returns its path-span array in search workspace. Before
reusing either workspace, copy any path and policy spans you still need into
caller-owned arrays. Copying only `TC_X509_search_result` leaves its pointers in
the original scratch arrays. The certificate DER stays in its shared buffers;
keep those buffers and the source snapshot alive.

## Client-certificate example

[examples/x509_client.c](../examples/x509_client.c) configures a client-authentication
purpose and digital-signature key usage. Its [header](../examples/x509_client.h)
defines caller-owned typed storage. The example accepts up to four certificates
of 4096 bytes each; its graph and name capacities are explicit in the storage type.
Adjust those limits to the certificates and memory available in your application.

Compile the example source with your application and link `tiny_crypto_c`. Supply
the trusted anchor, UTC validation time, signature provider and work budget to
`example_check_client_certificate`. It returns the same statuses as the library
API and leaves the result unchanged on failure. Only consume the result after
`TC_X509_PATH_VALID`; handle unsupported operations and limit failures separately
from invalid certificates.

Place `ExampleX509Workspace` in application-owned storage rather than on a
small task stack. Use a separate workspace for concurrent validations. Keep it
and the original certificate buffers alive while using the result. Revocation
evidence must be checked separately before granting application access.

The installed-consumer test copies this example outside the source tree and
builds C99 and C++11 callers against an installed library. The OpenSSL suite
also runs valid, invalid and work-limited signed-chain cases through it.

## Results

- `TC_X509_PATH_VALID`: the ordered-path checks succeeded; `out` is populated.
- `TC_X509_PATH_INVALID`: the certificate encoding or path requirements failed.
- `TC_X509_PATH_UNSUPPORTED`: a required algorithm, name rule or critical extension
  is not supported by the configured implementation.
- `TC_X509_PATH_LIMIT`: an input, workspace or processing limit was reached.
- `TC_X509_PATH_ERROR`: invalid arguments, overlapping storage or a provider error.

Only `VALID` authorizes use of the returned key under the supplied path settings.
Other statuses leave `out` unchanged. Workspace and provider state may change
on any result. A successful result does not establish revocation status.

## Verification

The signed-chain tests exercise the public API against OpenSSL verdicts, including
invalid signatures, validity periods, CA constraints, name constraints, policies,
usage restrictions and unknown critical extensions. See [Testing](testing.md)
for the OpenSSL test configuration and full-suite commands.
