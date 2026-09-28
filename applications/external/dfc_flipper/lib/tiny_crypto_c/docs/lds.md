<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# LDS security objects

Include `<tiny_crypto/lds.h>` and enable `TINY_CRYPTO_ENABLE_PIV_OBJECTS`.
`TC_LDS_read` parses the DER `LDSSecurityObject` carried inside CMS eContent,
as specified in [ICAO Doc 9303 Part 10](https://www.icao.int/sites/default/files/publications/DocSeries/9303_p10_cons_en.pdf).

The PIV/TWIC CMS Security Object profile uses content-type OID `1.3.27.1.1.1`,
specified by NIST SP 800-85B, AS06.04.06. Select `TC_PIV_CMS_SECURITY` to check
that envelope before relying on its LDS content.

Supply the encoded bytes, TLV limits, caller-owned frame storage and a work
budget. Check for `TC_TLV_OK` before using the returned object. Its spans borrow
the input, which must remain unchanged. Keep input, limits, frames, budget and
result storage disjoint. Argument errors preserve caller state; processing
failures may consume work and scratch, while preserving the result.

Versions 0 and 1 are supported. The parser requires 2–16 distinct data groups
numbered 1–16 and checks each digest's length against its declared SHA algorithm.
The `groups` bitmap has bit `n-1` set for group `n`. `hashes` holds the encoded
sequence of group-number/digest pairs. These remain in the input buffer.
Version 1 also supplies borrowed LDS and Unicode version strings.

For `TC_CMS_signed_data.content`, use `TC_LDS_read_content`. Pass the complete
OCTET STRING encoding, the same parsing limits and work budget, and optional
caller-owned byte storage. Single-chunk content is borrowed directly. Fragmented
BER content is joined in the supplied buffer, then checked as DER LDS data.
An encoded-content-sized buffer is sufficient; insufficient capacity returns
`TC_TLV_LIMIT`. Passing `NULL, 0` supports single-chunk content without a buffer.
Returned spans borrow either the original input or that buffer. Keep both stable
while using the result. Processing errors preserve the result but may change
the work counter, frame storage and byte buffer.

Use `TC_LDS_hash_find` to retrieve a group's digest. Pass the parsed object,
group number, limits, frames and shared work budget. `TC_TLV_OK` returns a
borrowed digest span; `TC_TLV_END` means the group is absent. Other results are
errors. The output remains unchanged unless a digest is returned. Lookup scans
at most 16 entries and uses the same schema checks as parsing. Keep the parsed
object and its backing bytes unchanged throughout lookup and verification.

`TC_LDS_hash_check` hashes an array of `TC_bytes` spans in order and compares
the complete digest with the selected group's value. This lets callers hash
shared buffers without concatenating them. Pass zero spans for empty content.
Check the return value first: `TC_TLV_OK` sets `matched` to 0 or 1,
`TC_TLV_END` means the group is absent, and other results indicate errors.
Errors preserve `matched`. The selected hash implementation must be enabled.
The work budget covers lookup, hashing, and comparison; TLV limits bound the
span count and total content length. Keep all inputs separate from frames,
the work counter, and the match result.

Hash identifiers accept absent or NULL parameters, as required for inspection
systems. Unknown algorithms or security-object versions return
`TC_TLV_UNSUPPORTED`. Hash metadata can be parsed with the corresponding hash
implementation disabled.

## PIV and TWIC containers

Include `<tiny_crypto/piv_security.h>` for `TC_PIV_security_read`. Select
`TC_PIV_SECURITY_CONTAINER` for a complete `53` response, or
`TC_PIV_SECURITY_CONTENTS` for its contents. Supply the input span and output
structure. The parser requires `BA`, nonempty `BB`, and empty `FE` in that order.
It returns borrowed mapping and CMS spans plus a group bitmap. Mapping records
contain a group number (1–16) and a two-byte, big-endian container ID. Repeated
groups or container IDs are rejected. The output changes only on `TC_TLV_OK`.

Use `TC_PIV_security_group_find` with a container ID to retrieve its group
number, then pass that number to `TC_LDS_hash_check`. A missing container returns
`TC_TLV_END`; errors preserve the output number. Require the container's `groups`
bitmap to equal the authenticated LDS object's `groups` before using the mapping.

The mapping is outside the CMS signature. Applications must reconcile it with
the authenticated LDS hash list and their required container policy.

Read the `BB` value with `TC_PIV_CMS_read` and `TC_PIV_CMS_SECURITY` to check
the single-signer security-object CMS profile. It requires attached content
with the ICAO LDS content type and signed attributes. The signing certificate
must be omitted. Both issuer/serial and subject-key-ID identifiers are accepted.
Supply the authenticated CHUID certificate as `signer_certificate` in the CMS
validation request, with zero detached-content spans. Apply the content-signing
usage, path, and revocation policy used for the CHUID. Parse the authenticated
eContent with `TC_LDS_read` before checking object hashes.

`TC_PIV_security_validate` in `<tiny_crypto/credential.h>` composes these
steps with signer path and revocation validation. Supply a
`TC_PIV_security_validation_request` containing the security object,
authenticated CHUID
signer certificate, card profile and expiration, and a complete array of
`TC_PIV_security_data` records. Each record associates a container ID with the
ordered spans to hash. The operation rejects missing, extra, and repeated containers,
unequal mapping/LDS group sets, and digest mismatches.

Supply the shared `TC_validation_context` plus a bounded
`TC_PIV_security_validation_workspace` LDS content buffer. Keep the request,
object data, trust source, CRLs, policy and work counter separate from mutable
scratch. Accept only `TC_CREDENTIAL_VALID`. Applications must select the required
inventory and exact protocol-defined hash inputs, including any framing or
decryption, before calling this operation.

TWIC's unsigned CHUID remains a separate object. Use
`TC_TWIC_unsigned_CHUID_validate` to require container `0x3002` in the validated
inventory, compare its exact ordered parts with the supplied CHUID, and bind the
authenticated FASC-N, GUID and expiration to the card certificate. The ordinary
`TC_PIV_CHUID_validate` operation continues to require a signed CHUID.

For TWIC, the [TSA reader/card specification, section 11.2 note 4](https://www.ports.org/files/PDFs/TWIC%20Reader%20Hardware%20%26%20Card%20Application%20Specification.pdf)
defines hashes over stored object contents. Section 11.3 wraps those contents in
`53` for GET DATA. Hash the response's value bytes, retaining the inner field
tags and lengths, including CHUID's `FE 00`. Keep encrypted fields in their
stored form for this check. Privacy-key decryption uses separate working storage
when the encrypted bytes are still needed for validation.

Authenticate the containing CMS signature and signer path before relying on the
hash list. PIV and TWIC use the CHUID signing key and an external `BA` mapping
from data-group numbers to container IDs. The credential layer must apply that
mapping, define the exact object bytes to hash, and check the required objects.
