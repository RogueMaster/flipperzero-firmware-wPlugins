<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# AES key wrap

Enable `TINY_CRYPTO_ENABLE_AES` and `TINY_CRYPTO_AES_ENABLE_KW` in CMake. Include
`<tiny_crypto/aes_kw.h>` for C or `<tiny_crypto/aes_kw.hpp>` for C++.

The API implements the NIST SP 800-38F key wrap functions with AES as the
block cipher: KW-AE and KW-AD (RFC 3394) and KWP-AE and KWP-AD (RFC 5649).
Each call is one-shot and protects the confidentiality and integrity of key
data under a key-encryption key (KEK).

## Quick start

`TC_AES_KW_wrap` wraps key data that is a multiple of 8 bytes.
`TC_AES_KW_unwrap` recovers it and checks its integrity. The
[C example](../examples/aes_kw.c) runs the RFC 3394 section 4.1 example, a
KWP round trip and a rejected modified wrap, and `test_aes_kw_example` builds
and runs it.

```c
#include <tiny_crypto/aes_kw.h>

/* Wrap a 32-byte content key for storage, then recover it. */
TC_status store_and_recover(TC_bytes kek, const uint8_t content_key[32], uint8_t recovered[32])
{
  uint8_t wrapped[TC_AES_KW_WRAPPED_BYTES(32)];
  TC_status status = TC_AES_KW_wrap(kek, (TC_bytes){content_key, 32},
                                    (TC_buffer){wrapped, sizeof wrapped});
  if (status != TC_OK)
    return status; /* unsupported KEK length or bad arguments */
  status = TC_AES_KW_unwrap(kek, (TC_bytes){wrapped, sizeof wrapped},
                            (TC_buffer){recovered, 32});
  /* TC_MISMATCH: the integrity check failed and recovered holds zeros. */
  return status;
}
```

## Choosing KW or KWP

KW wraps key data of at least 16 bytes in whole 8-byte semiblocks and adds
one semiblock. KWP wraps any nonempty length. It pads the key data with zeros
to a semiblock boundary and records the length in the integrity check value,
so `TC_AES_KWP_unwrap` returns the original length. Use the function that the
protocol names. The two formats use different integrity check values, so a KW
wrap fails a KWP unwrap and the reverse.

## Key-encryption keys

| Build                                | Accepted KEK lengths  |
| ------------------------------------ | --------------------- |
| `TINY_CRYPTO_AES_ENABLE_DYNAMIC=OFF` | `TC_AES_KEYLEN` bytes |
| `TINY_CRYPTO_AES_ENABLE_DYNAMIC=ON`  | 16, 24 or 32 bytes    |

`TC_AES_KW_KEK_LENGTH_SUPPORTED(n)` expands to the same rule, so an
application can check its KEK size at compile time. Fixed-key builds keep one
176-byte schedule at AES-128. A KEK of another length returns `TC_ERROR`
before any write. Keep the KEK secret (SP 800-38F section 5.1).

## Buffer sizes

| Macro                          | Value                                          |
| ------------------------------ | ---------------------------------------------- |
| `TC_AES_KW_WRAPPED_BYTES(n)`   | `n + 8` for `n` bytes of KW key data           |
| `TC_AES_KWP_WRAPPED_BYTES(n)`  | `n` rounded up to a multiple of 8, plus 8      |
| `TC_AES_KW_UNWRAPPED_BYTES(w)` | `w - 8` for `w` wrapped bytes, `w` at least 16 |

Wrap writes exactly the wrapped size. KW unwrap writes `wrapped.length - 8`
bytes. KWP unwrap uses `wrapped.length - 8` bytes of the output as its working
area and reports the key data length through `key_data_length`. After
`TC_OK`, the bytes from that length to the end of the working area hold the
verified zero padding. Output capacity may exceed these sizes. Bytes past
them stay untouched.

## In-place use

Inputs and outputs may overlap in any way. Each call reads its inputs in full
before its first output write. That covers an exact alias, the RFC 3394 layout
with key data at `wrapped.data + 8`, and a KEK stored inside the output
buffer. `key_data_length` must lie outside the KWP working area.

## Results and failure

| Status        | Meaning                                                                                                                  |
| ------------- | ------------------------------------------------------------------------------------------------------------------------ |
| `TC_OK`       | The wrap succeeded, or the unwrapped key data passed its integrity check.                                                |
| `TC_MISMATCH` | Unwrap only: the ICV, the KWP length indicator or the KWP padding failed its check.                                      |
| `TC_ERROR`    | A NULL or unsupported argument, a length outside the domain, a short output, a key schedule failure or a cipher failure. |

Unwrap runs the inverse wrapping function in the output and checks the
integrity value afterwards. The output holds unverified bytes only during the
call. Keep it private to the caller until the call returns. Every failure
after the argument checks wipes the working area, and a failed KWP unwrap
leaves `*key_data_length` unchanged. A cipher failure during wrap wipes the
wrapped output. Argument errors, including a received wrapped length outside
the domain, return `TC_ERROR` before any write.

Protocol code treats `TC_ERROR` and `TC_MISMATCH` from an unwrap alike as a
rejection. SP 800-38F Appendix A.3 asks implementations to monitor failed
unwrap attempts per KEK and limit them where needed.

## Length limits

| Function            | Accepted input                      | Limit source              |
| ------------------- | ----------------------------------- | ------------------------- |
| `TC_AES_KW_wrap`    | 16 to 2^57 - 8 bytes, multiple of 8 | SP 800-38F Table 1        |
| `TC_AES_KW_unwrap`  | 24 to 2^57 bytes, multiple of 8     | SP 800-38F Table 1        |
| `TC_AES_KWP_wrap`   | 1 to 2^32 - 8 bytes                 | Section 5.3.2 restriction |
| `TC_AES_KWP_unwrap` | 16 to 2^32 bytes, multiple of 8     | SP 800-38F Table 1        |

The `TC_AES_KW_MAX_*` and `TC_AES_KWP_MAX_*` macros hold these values. Table 1
lists KWP plaintext up to 2^32 - 1 bytes. Plaintext above 2^32 - 8 bytes
wraps to more than the 2^29-semiblock KWP ciphertext limit. Section 5.3.2
lets an implementation restrict the valid lengths and asks that wrap outputs
correspond to unwrap inputs, so KWP wrap stops at 2^32 - 8 bytes. On 16-bit
and 32-bit targets `size_t` sets a lower bound, and a wrapped size above
`SIZE_MAX` returns `TC_ERROR`.

## Resources

Each call expands the KEK on the stack: 176 bytes at AES-128 in fixed-key
builds and 240 bytes with dynamic keys. The calls use no heap and no
workspace. Unwrap needs the AES inverse cipher, so enabling key wrap links it
even in wrap-only firmware. The inverse cipher adds a 256-byte table in RAM in
the runtime S-box profile and in flash in the fast profile. The constant-time
profile computes it. Firmware that calls only KW drops the KWP code through
section garbage collection.

## Timing

The ICV, length indicator and padding checks run in constant time and end in
one branch. Lengths and the KEK size are public. A wrap or unwrap of `n`
semiblocks costs 6(n - 1) block operations, and KWP with two semiblocks costs
one.

## Scope

The API covers the default ICVs of SP 800-38F with the AES forward cipher as
the designated cipher function. TDEA key wrap (TKW), the alternative initial
values of RFC 3394 section 2.2.3.2 and the inverse-cipher designation of
SP 800-38F section 5.1 are outside it. CMS KEKRecipientInfo parsing and the
`id-aes*-wrap-pad` identifiers of RFC 5649 section 5 belong to a CMS
consumer.

## Testing

`test_kw` runs in `test_aes`, `test_aes_192`, `test_aes_256` and
`test_aes_dynamic`: the RFC 3394 section 4 and RFC 5649 section 6 examples,
argument and limit checks, bit-flip integrity checks, every KWP length
indicator and padding case for two to four semiblocks, and overlap. With
`TINY_CRYPTO_TEST_FULL=ON` it also runs the NIST CAVP KWVS files in
`tests/vectors/aes/kw/`. `test_wycheproof_keywrap` runs the Wycheproof
`aes_wrap_test.json` and `aes_kwp_test.json` suites against each KEK size.
`test_aes_backend_failure` fails each block operation in turn,
`test_aes_runtime_sbox` checks calls before `TC_AES_init_sbox`, and
`test_aes_kw_sbox_*_qemu_avr` runs on an emulated ATmega328P when
`qemu-system-avr` is installed. It checks the RFC 3394 section 4.1 example, KWP
round trips and an in-place unwrap and rewrap of a 392-byte Wycheproof vector.
