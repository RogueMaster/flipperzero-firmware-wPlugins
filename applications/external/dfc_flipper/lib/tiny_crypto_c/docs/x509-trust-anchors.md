<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# Trust anchors

Enable `TINY_CRYPTO_ENABLE_TRUST_ANCHOR_FORMAT` to read RFC 5914
`TrustAnchorList` DER. The list accepts certificate, TBS certificate, and
`TrustAnchorInfo` choices when their corresponding `TINY_CRYPTO_TAF_ENABLE_*` build
options are enabled. The format reader returns views that borrow the original DER.

RFC 5914 defines the encoded anchor and its path controls. RFC 5937 describes
how those controls constrain X.509 path validation. The implementation always
enforces constraints on a selected anchor. See the
[RFC 5914](https://www.rfc-editor.org/info/rfc5914/) and
[RFC 5937](https://www.rfc-editor.org/info/rfc5937/) publications.

## Read and publish a list

Bind the list, parsing limits and a bounded `TC_X509_workspace` to a
`TC_X509_trust_anchor_reader` with `TC_X509_trust_anchor_list_init`, then
call `TC_X509_trust_anchor_next` until it returns `TC_TLV_END`. Store each
result in a caller-owned array of `TC_X509_store_anchor` records. The reader
uses the workspace on every call, so keep it alive and unshared until the
last call. The reader, the frames and the extension OID array must be pairwise
disjoint and lie outside the list and the workspace struct. init returns
`TC_TLV_ARGUMENT` for any overlap. A disabled choice returns
`TC_TLV_UNSUPPORTED`.

```c
enum { ANCHOR_CAPACITY = 8 };
TC_TLV_frame frames[16];
TC_bytes oids[32];
const TC_X509_workspace parser = {{frames, 16}, oids, 32};
const TC_TLV_limits limits = {16384, 16384, 2048, 16};
TC_X509_store_anchor anchors[ANCHOR_CAPACITY], record;
TC_X509_trust_anchor_reader reader;
size_t count = 0;
TC_TLV_result result = TC_X509_trust_anchor_list_init(&reader, list, &limits, &parser);
if (result != TC_TLV_OK)
  return result; /* ARGUMENT, INVALID or LIMIT. */
while ((result = TC_X509_trust_anchor_next(&reader, &record)) == TC_TLV_OK) {
  if (count == ANCHOR_CAPACITY)
    return TC_TLV_LIMIT;
  anchors[count++] = record;
}
if (result != TC_TLV_END)
  return result; /* A malformed, unsupported or oversized anchor. */
```

Here `list` is a `TC_bytes` span over the DER TrustAnchorList. A valid
`TrustAnchorInfo` without `certPath` is marked `x509_unusable` and cannot
authorize an X.509 path.

Certificate, TBS certificate and embedded `CertPathControls` certificates
follow the `TC_X509_read` field rules. An anchor also needs a non-empty
subject, because it issues certificates (RFC 5280 section 4.1.2.6), a
validity period that starts at or before its end, and keyCertSign in a
present keyUsage extension. Other records are `TC_TLV_INVALID`. Path
validation ignores the anchor's validity period.

Keep the DER and record array immutable while any validation uses them.
`TC_X509_store_array_source` turns the records and untrusted candidate
certificate spans into a `TC_X509_store_source`. The application must
authenticate and authorize the list before calling `TC_X509_store_publish`.
Parsing a list or receiving it inside CMS does not grant trust. If the list
arrives as CMS SignedData, verify its signature and content type using an
independent bootstrap trust context before passing the extracted DER list to
this reader.

The store retains borrowed source configuration. Follow the snapshot lifetime
rules in [Certificate store](x509-store.md). The backing DER may be reclaimed
only after the last reader releases its snapshot. Network retrieval and
persistent-storage updates belong to the application.

## Build an anchor from a certificate

`TC_X509_store_anchor_from_certificate` turns a parsed root certificate into a
`TC_X509_store_anchor` with the rules of the list's certificate choice. It
needs the path module only. The record's `names`, `policy_set`,
`policy_flags` and `path_len` come from the root's nameConstraints,
certificatePolicies, policyConstraints, inhibitAnyPolicy and basicConstraints
extensions. The record borrows the root DER, so keep that DER unchanged while
the record is in use.

```c
TC_X509_certificate root;
TC_X509_store_anchor anchor;
if (TC_X509_read((TC_bytes){der, der_length}, &limits, &parser, &root) != TC_TLV_OK ||
    TC_X509_store_anchor_from_certificate(&root, &limits, &parser, &anchor) != TC_TLV_OK)
  return 0; /* ARGUMENT leaves anchor unchanged. Other failures zero it. */
```

Use the builder for application-provisioned roots. A record that only
copies the subject and public key, with `certificate_extensions` left empty,
validates without the root's constraints.

[Synthetic TWIC validation](../tests/twic/synthetic_validation.c) is an
executable example of bounded parsing, caller-owned records, explicit store
publication, and path validation against a constrained anchor.

## Path controls

An anchor's policies intersect the application's initial policies. A
`TrustAnchorInfo` policySet lists unique policy identifiers (RFC 5280
section 4.2.1.4) without policyQualifiers (RFC 5914 section 2.5), and an
anchor certificate's certificatePolicies extension must also list unique
identifiers. Its
permitted names intersect the application's permitted names, and excluded
names from both sources apply. Restrictive policy flags combine with the
application flags. An anchor path-length limit counts non-self-issued
intermediate CAs. Unknown critical anchor extensions prevent validation.
An inhibitAnyPolicy or policyConstraints field on the anchor sets the
corresponding Boolean path input. Its SkipCerts count is ignored, as
RFC 5937 section 2 specifies.

Validation applies the record fields `names`, `policy_set`, `policy_flags`
and `path_len`. It also checks `certificate_extensions`, where a path
control whose record field is empty returns `TC_X509_PATH_UNSUPPORTED`.
The control would otherwise be dropped (RFC 5914 section 2.5). This covers
nameConstraints without subtrees in `names`, certificatePolicies without
`policy_set`, a policyConstraints or inhibitAnyPolicy field without its
policy flag, and a basicConstraints pathLen without `has_path_len`.

For `TrustAnchorInfo`, its path-control fields take precedence over matching
extensions in its embedded certificate (RFC 5914 section 2.5). The reader
records each `CertPathControls` field it applied in `replaced_controls`:
`TC_X509_ANCHOR_REPLACED_POLICY_SET` for policySet,
`TC_X509_ANCHOR_REPLACED_POLICY_FLAGS` for policyFlags,
`TC_X509_ANCHOR_REPLACED_NAMES` for nameConstr and
`TC_X509_ANCHOR_REPLACED_PATH_LEN` for pathLenConstraint. Validation then
uses the record field in place of the matching `certificate_extensions`
control. A `policyFlags` field that clears a flag set by the embedded
certificate's policyConstraints or inhibitAnyPolicy therefore validates with
the cleared flag. A caller-built record sets a bit only when its field holds
the replacing value. Without the bit, the same record returns
`TC_X509_PATH_UNSUPPORTED`. The `CertPathControls` values are always
enforced. A basicConstraints pathLen in `exts` can only lower
`pathLenConstraint`, so the reader keeps the smaller value. A caller-built
record whose `extensions` pathLen is below its `path_len`, or that has no
`path_len`, returns `TC_X509_PATH_UNSUPPORTED`.

RFC 5914 section 2.6 forbids certificatePolicies, policyConstraints,
inhibitAnyPolicy and nameConstraints in the `exts` field, because
`CertPathControls` carries them. Such an extension makes the anchor
`TC_TLV_INVALID` when parsed and `TC_X509_PATH_INVALID` when a caller-built
anchor carries it in `extensions`. A `TrustAnchorInfo` embedded certificate
must match the stated name and public key, and any subject key identifier
must match the anchor key identifier. The anchor record selected by path
search applies only to that attempted path. Another anchor cannot relax its
controls.

Validation also requires a signature provider, an application-supplied UTC
time, and bounded path workspaces. Revocation is a separate check. A root
certificate present only among untrusted candidates never becomes an anchor.

## Resource options

The three choice options can be disabled independently to remove their
decoders. At least one must be enabled when the format module is enabled.
The default resource profile leaves the format module disabled. The desktop
profile includes all choices. Invalid feature combinations fail at compile
time and in CMake configuration.

Run the vendored parser, path, and TWIC tests with `ctest --output-on-failure`
from a configured build directory. The mandatory tests use the committed DER
fixtures and run without OpenSSL.
