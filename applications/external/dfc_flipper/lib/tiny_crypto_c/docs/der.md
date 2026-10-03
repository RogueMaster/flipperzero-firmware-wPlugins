<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# DER values

Enable `TINY_CRYPTO_ENABLE_TLV=ON` and `TINY_CRYPTO_ENABLE_DER=ON`, then
include `<tiny_crypto/der.h>`. The readers check one complete DER value and
return borrowed views of its contents. They build on the
[TLV readers](tlv.md) and share `TC_TLV_result`.

## Quick start

```c
#include <tiny_crypto/der.h>

/* Read an ECDSA-Sig-Value and an RSAPublicKey from received DER. */
TC_TLV_result read_key_and_signature(TC_bytes key_der, TC_bytes signature_der,
                                     TC_DER_rsa_public_key* key,
                                     TC_DER_signature_pair* signature)
{
  TC_DER_rsa_public_key parsed_key;
  TC_DER_signature_pair parsed_signature;
  TC_TLV_result result = TC_DER_rsa_public(key_der, &parsed_key);
  if (result != TC_TLV_OK)
    return result; /* INVALID for malformed input, ARGUMENT for a bad span */
  result = TC_DER_ecdsa_signature(signature_der, &parsed_signature);
  if (result != TC_TLV_OK)
    return result;
  /* Both results borrow the input spans. Keep them unchanged while in use. */
  *key = parsed_key;
  *signature = parsed_signature;
  return TC_TLV_OK;
}
```

The readers allocate nothing and need no frames, work budget or workspace.
Pass the exact encoding: a truncated value, trailing bytes after it or a
different tag returns `TC_TLV_INVALID`.

## Readers

| Function                        | ASN.1 type                                        | Output                                    |
| ------------------------------- | ------------------------------------------------- | ----------------------------------------- |
| `TC_DER_integer`                | INTEGER                                           | two's-complement contents and a sign flag |
| `TC_DER_positive_integer`       | INTEGER greater than zero                         | magnitude without a sign octet            |
| `TC_DER_uint32`                 | INTEGER from 0 to `UINT32_MAX`                    | `uint32_t`                                |
| `TC_DER_bit_string`             | BIT STRING                                        | payload bytes and the unused-bit count    |
| `TC_DER_oid`                    | OBJECT IDENTIFIER                                 | encoded contents                          |
| `TC_DER_boolean`                | BOOLEAN                                           | 0 or 1                                    |
| `TC_DER_null`                   | NULL                                              | none                                      |
| `TC_DER_sequence`, `TC_DER_set` | SEQUENCE, SET                                     | contents octets                           |
| `TC_DER_algorithm_identifier`   | AlgorithmIdentifier                               | OID and complete parameter encoding       |
| `TC_DER_subject_public_key`     | SubjectPublicKeyInfo                              | algorithm and key bytes                   |
| `TC_DER_private_key_info`       | PKCS #8 PrivateKeyInfo, RFC 5958 OneAsymmetricKey | algorithm, key, attributes, public key    |
| `TC_DER_rsa_public`             | PKCS #1 RSAPublicKey                              | modulus and exponent magnitudes           |
| `TC_DER_rsa_private`            | PKCS #1 two-prime RSAPrivateKey                   | eight component magnitudes                |
| `TC_DER_ecdsa_signature`        | ECDSA-Sig-Value                                   | `r` and `s` magnitudes                    |

`TC_DER_integer_contents`, `TC_DER_uint32_contents` and `TC_DER_oid_contents`
check the contents octets of an IMPLICIT-tagged value. Use them after a TLV
reader has checked the context-specific tag and length.

OID contents stay encoded, so an OID of any arc size needs no integer
conversion. Compare OIDs as byte spans. Integer magnitudes also stay encoded
for the RSA and EC code that consumes them.

## Results and failure state

Every reader returns one of these values:

- `TC_TLV_OK`: the encoding is valid and the outputs are written.
- `TC_TLV_INVALID`: malformed, truncated or trailing input, a wrong tag, a
  tag or length field wider than the build parses, or a value that breaks a
  DER rule below.
- `TC_TLV_LIMIT`: an INTEGER above `UINT32_MAX` for `TC_DER_uint32` and
  `TC_DER_uint32_contents`.
- `TC_TLV_UNSUPPORTED`: PKCS #1 version 1 (multi-prime) and PKCS #8 versions
  above 1, including versions above `UINT32_MAX`.
- `TC_TLV_ARGUMENT`: a NULL output, an output that overlaps the input or
  another output, or a span with NULL data and a nonzero length.

Outputs are unchanged on every failure. `TC_TLV_END` and `TC_TLV_MORE` are
never returned.

## Conformance

The checks follow ITU-T X.690 (02/2021):

- Lengths are definite and minimal (section 10.1), checked by the DER TLV
  reader. The readers accept single-octet universal tags.
- INTEGER contents are nonempty, and the first nine bits are never all zero or
  all one (section 8.3.2). `TC_DER_integer` keeps a needed sign octet in the
  returned span, so signed data stays byte-exact.
- BOOLEAN is one octet, `00` or `FF` (sections 8.2 and 11.1).
- BIT STRING has an initial octet from 0 to 7, zero for an empty payload, and
  its unused bits are zero (sections 8.6.2 and 11.2).
  `TC_DER_subject_public_key` also requires a nonempty, byte-aligned key.
- NULL has no contents (section 8.8.2).
- OBJECT IDENTIFIER subidentifiers use minimal base-128 encoding, and the last
  octet ends a subidentifier (section 8.19.2).
- SEQUENCE and SET contents are returned unparsed (sections 8.9 and 8.11). The
  caller checks SET OF ordering (section 11.6) and schema rules such as DEFAULT
  values (section 11.5).

The structure readers check the container fields of RFC 5280 section 4.1.1.2
(AlgorithmIdentifier), RFC 5280 section 4.1.2.7 (SubjectPublicKeyInfo), RFC
8017 appendices A.1.1 and A.1.2 (RSA keys), RFC 5958 section 2 (asymmetric key
packages) and RFC 3279 section 2.2.3 (ECDSA signatures).

## Limitations

- The readers check encodings. Callers validate algorithm parameters, key
  mathematics, public and private key consistency, attribute schemas and
  signature ranges against the group order.
- PKCS #8 version 1 requires its public key. Version 0 rejects one.
- Readers take complete values only. Use the [TLV stream reader](tlv.md) for
  fragmented input.
- Encoders are outside this module.

## Tests

`test_tlv_full` runs the DER reader tests, including malformed, truncated and
unsupported-version encodings. `test_tlv_core` checks a build without DER, BER and the
stream reader. `fuzz_tlv` exercises the DER readers under libFuzzer. Run the
unit tests with:

```sh
ctest --test-dir build -R '^test_tlv_(full|core)$' --output-on-failure
```
