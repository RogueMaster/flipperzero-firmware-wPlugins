<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# X.509 CRL parsing

Include `<tiny_crypto/x509_crl.h>` and enable
`TINY_CRYPTO_ENABLE_X509_REVOCATION`.
`TC_X509_crl_read` reads a DER `CertificateList` into borrowed spans and decoded
update times. Revocation decisions use the separate path revocation API.

Pass the encoded CRL, parsing limits, frames with one entry per nesting level,
a work budget and a result:

```c
enum { CRL_MAX_BYTES = 65536, CRL_MAX_ELEMENTS = 8192,
       CRL_MAX_DEPTH = 32, CRL_WORK = 1000000 };
TC_TLV_frame frames[CRL_MAX_DEPTH];
const TC_TLV_limits limits = {
    CRL_MAX_BYTES, CRL_MAX_BYTES, CRL_MAX_ELEMENTS, CRL_MAX_DEPTH
};
size_t work = CRL_WORK;
TC_X509_crl crl;
TC_TLV_result result = TC_X509_crl_read(
    encoded, &limits, (TC_TLV_frames){frames, CRL_MAX_DEPTH}, &work, &crl);
if (result != TC_TLV_OK) {
    /* Handle malformed input, exhausted limits or invalid arguments. */
    return result;
}
```

Here `encoded` is a `TC_bytes` containing one complete CRL. Choose limits for
your issuer's CRLs and keep the frame array outside a small task stack if needed.
The work counter is consumed on parsing failures as well as success.

`encoded` and `tbs` retain the complete CRL and signed TBSCertList. `issuer` is
the DER Name. `signature` contains the signature bytes without BIT STRING
framing. `revoked` and `extensions` retain their SEQUENCE wrappers and are empty
when absent. `version` is 1 or 2. Read `next_update` only when `has_next_update`
is set.

Keep the input bytes alive and unchanged while using the result. Frame scratch
may be reused after the call. Input, limits, frames, work and result storage must
not overlap. A failed call leaves the result untouched.

Extension interpretation, signature verification, signer trust, freshness,
scope and base/delta selection are separate from this reader. Use
[path revocation checking](x509-revocation.md) for a validated certificate path.

## CRL extensions

`TC_X509_crl_extensions_read` decodes CRL-level extensions into a
`TC_X509_crl_extensions` view. Pass `crl.extensions` and a `TC_X509_workspace`:

```c
enum { CRL_EXTENSION_CAPACITY = 16 };
TC_bytes extension_oids[CRL_EXTENSION_CAPACITY];
const TC_X509_workspace workspace = {
    {frames, CRL_MAX_DEPTH}, extension_oids, CRL_EXTENSION_CAPACITY
};
TC_X509_crl_extensions extensions;
result = TC_X509_crl_extensions_read(
    crl.extensions, &limits, &workspace, &work, &extensions);
if (result != TC_TLV_OK) {
    return result;
}
```

The OID array needs one slot per extension and is reused to detect duplicates.
It may be reused after the call because returned spans borrow the CRL.
An absent extension sequence produces an empty view.

`present` and `critical` use the `TC_X509_CRL_EXT_*` masks. `number` and
`base_number` contain DER INTEGER contents. `distribution` describes the issuing
distribution point, including reason and certificate-type restrictions.
`freshest` retains a CRLDistributionPoints sequence. `issuer_alt` contains
GeneralNames fields without their SEQUENCE wrapper.

An unrecognized critical extension is reported in `unknown_critical_oid`.
Revocation policy decides whether decoded criticality and scope are acceptable.

## Indexing a collection

`TC_X509_crl_index_init` accepts an array of DER spans and fills a caller-owned
`TC_X509_crl_record` array. Pass the same parsing workspace used above, one work
budget for the collection, and a `TC_X509_crl_index` result. Record capacity must
cover the number of input spans. Parsing limits apply separately to each CRL.

```c
const TC_bytes inputs[] = {encoded};
TC_X509_crl_record records[1];
TC_X509_crl_index index;
work = CRL_WORK;
result = TC_X509_crl_index_init(inputs, 1, &limits, &workspace, &work,
                                records, 1, &index);
if (result != TC_TLV_OK) {
    return result;
}
```

The index borrows the record array, and each record borrows its original CRL
bytes. Keep both unchanged while using the index. Frame and OID scratch may be
reused after initialization. Empty input accepts NULL/0 input and record arrays.

Malformed CRLs stop initialization without changing the index result. Record
storage may be partially populated. Extension-policy failures are retained in
records so later selection can evaluate alternatives. The revocation resolver
checks signature validity, freshness and trust.

## File and flash sources

Include `<tiny_crypto/x509_crl_source.h>` for CRLs held outside RAM. A `TC_source`
supplies a 64-bit length and an exact read-at callback. Return `TC_ERROR` for a
short read or storage failure, and the preparation call returns `TC_TLV_IO`.
Keep the source unchanged throughout preparation.

`TC_X509_crl_prepare_begin` takes the source, target serial/issuer pairs, limits
and a caller-owned workspace. Use `TC_X509_crl_prepare_size` and
`TC_X509_crl_prepare_alignment` to size its state buffer. `TC_X509_crl_storage`
provides alignment for static storage. The remaining workspace contains a read
window, metadata buffer, entry scratch, retained issuer storage, parser/name
scratch and one match slot per target.

Call `TC_X509_crl_prepare_step` until `complete` is set. Each call has explicit
entry and hash-byte limits and consumes a per-call work budget. Refill that
budget before the next call. Physical read-byte and callback limits apply to the
whole job. Preparation scans entries, then hashes the exact signed encoding.
Individual metadata and entry limits stay independent of the complete CRL size.
A CRL whose extensions fail policy, such as an unknown critical extension,
still prepares. Its record keeps `TC_TLV_INVALID` or `TC_TLV_UNSUPPORTED` in
`policy`, its entries are left unscanned and every target is unmatched. The
resolver skips that record, as it does for the same CRL from
`TC_X509_crl_index_init` (RFC 5280 section 5.2).

After completion, `TC_X509_crl_prepare_finish` fills a `TC_X509_crl_record` for
an index. Its digest and queried matches remain in job storage. The revocation
resolver verifies the signature and applies signer trust, freshness, distribution
scope and base/delta policy. Include targets for every path member and candidate
CRL signer whose revocation may be checked. A target absent from the prepared
batch produces `TC_TLV_UNSUPPORTED`.

The certificate source must include the CRL signer certificates, including a
root certificate when that root signs a CRL. Trust anchors supply trusted names
and keys. Signer certificates supply the extensions used in signer selection.
Equal-number delta CRLs with different retained hash algorithms produce
`TC_TLV_UNSUPPORTED` when their signed contents must be compared.

Keep job state, metadata, targets and matches unchanged until the last record
use. Read-window, entry, issuer and parser scratch can be reused after preparation.
The completed record retains the required evidence in RAM, so its source can be
released. Call `TC_X509_crl_prepare_clear` after releasing all borrowed records.
Preparation failures leave scratch provisional and require a new job.
