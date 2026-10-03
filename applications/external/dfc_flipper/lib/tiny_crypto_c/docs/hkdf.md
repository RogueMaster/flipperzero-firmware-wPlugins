<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# HKDF

Enable `TINY_CRYPTO_ENABLE_HMAC`, `TINY_CRYPTO_ENABLE_HKDF`, and at least one
SHA family in CMake. Include `<tiny_crypto/hkdf.h>` for C or
`<tiny_crypto/hkdf.hpp>` for C++.

The API implements RFC 5869 extract and expand using HMAC-SHA-1, SHA-224,
SHA-256, SHA-384, or SHA-512 when the corresponding hash is enabled. Each
enabled hash has `TC_HKDF_SHA*_extract`, `TC_HKDF_SHA*_expand` and
`TC_HKDF_SHA*_derive`.

## Quick start

`TC_HKDF_SHA256_derive` produces one output. It extracts a PRK, expands the
requested key, then clears the PRK. The [C example](../examples/hkdf.c) makes
the same call on the RFC 5869 appendix A.1 inputs, and `test_hkdf_example`
builds and runs it.

```c
#include <tiny_crypto/hkdf.h>

/* Derive a 32-byte session key from a shared secret. */
TC_status derive_session_key(const uint8_t* secret, size_t secret_length,
                             const uint8_t* salt, size_t salt_length,
                             uint8_t key[32])
{
  static const uint8_t info[] = "example session key v1";
  /* The input keying material is a list of spans. A hybrid secret Z || T
   * from SP 800-56C revision 2 passes as two entries. */
  const TC_bytes ikm[] = {{secret, secret_length}};
  const TC_status status =
      TC_HKDF_SHA256_derive((TC_bytes){salt, salt_length}, ikm, 1,
                            (TC_bytes){info, sizeof info - 1}, (TC_buffer){key, 32});

  /* Argument errors leave key unchanged. Later failures wipe it. */
  return status;
}
```

The caller wipes the secret with `TC_secure_zero` when it is no longer needed,
and wipes `key` when the session ends.

## Inputs and outputs

`extract` and `derive` take the input keying material as an array of
`TC_bytes` spans, which the HMAC reads in order as one concatenated input. Pass
one span for an ordinary secret.

For NIST SP 800-56C revision 1, pass the shared secret `Z` as the input keying
material and the protocol's encoded `FixedInfo` as `info`. Revision 2 permits a
hybrid shared secret `Z || T`: pass `Z` and `T` as two spans. The library reads
them in place without assembling another buffer. Applications construct
`FixedInfo` according to their protocol. The ACVP adapter's
`uPartyInfo || vPartyInfo || l` encoding belongs to the test fixture.

For several outputs from the same shared secret, call `extract` once, then
`expand` with a distinct `info` value for each output. Clear the PRK with
`TC_secure_zero` after the last expansion. Limit its use to this derivation
operation. Use domain-separated `info` values for different keys.

Inputs are `TC_bytes` spans and outputs are `TC_buffer` storage. All lengths
are bytes. `extract` writes one hash digest to a caller-owned PRK array.
`expand` and `derive` write exactly `output.capacity` bytes, 1 through
`255 * HashLen`. `expand` needs a PRK of at least one digest. A span may have
NULL data when its length is zero, and a zero `ikm_count` is an empty input.
An empty salt has the RFC's all-zero HMAC key effect.

## Failure behavior

Outputs stay disjoint from every input span and from the `ikm` array.
Invalid arguments return `TC_ERROR` and leave output unchanged. A failure after
processing begins clears output. The caller owns every input and output buffer
and keeps inputs stable until the function returns. The functions return
`TC_OK` or `TC_ERROR`. See the [failure and wipe rules](api.md#failure-state-and-wiping).

## Conformance and limitations

The NIST [SP 800-56C revisions 1 and 2](https://csrc.nist.gov/pubs/sp/800/56/c/r2/final)
cover more extraction and expansion combinations than this HKDF API. The
implemented scope is HMAC-based HKDF for the enabled SHA families. The pinned
[ACVP corpus](../tests/vectors/kdf/acvp_hkdf/README.md) checks SHA2-224/256/384/512
for both revisions. The library holds no ACVP or FIPS validation.
