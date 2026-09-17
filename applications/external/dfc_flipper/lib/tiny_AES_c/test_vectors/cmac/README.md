<!--
SPDX-FileCopyrightText: Mistial Dev
SPDX-License-Identifier: Unlicense
-->

# AES-CMAC test vectors

## NIST SP 800-38B Appendix D

The four AES-128 / AES-192 / AES-256 examples for empty, one-block, multi-block
partial, and multi-block full messages are transcribed into `cmac_test.c` from
[NIST SP 800-38B](https://csrc.nist.gov/publications/detail/sp/800-38b/final)
Appendix D. Used only for interoperability testing.

## NIST CAVP (CMAC Gen / Ver) — full AES corpora

These files are the complete AES entries from the
[NIST CAVP CMAC test vectors](https://csrc.nist.gov/projects/cryptographic-algorithm-validation-program/message-authentication)
archive (`cmactestvectors.zip`, CAVS 11.0), unchanged:

| File | Cases |
|------|------:|
| `CMACGenAES128.rsp` | 96 |
| `CMACGenAES192.rsp` | 144 |
| `CMACGenAES256.rsp` | 96 |
| `CMACVerAES128.rsp` | 240 |
| `CMACVerAES192.rsp` | 360 |
| `CMACVerAES256.rsp` | 240 |

Includes short tags (`Tlen` 4/5) and 64 KiB messages. TDES rsp files from the
zip are not used (this library is AES-only).

CMAC unit-test builds set `AES_CMAC_MIN_TAG_LEN=4` so every CAVP row can exercise
the public API. The **product default** remains 8 (SP 800-38B ≥ 64-bit
guidance; same floor style as EAX).

## Wycheproof — full file

`aes_cmac_test.json` is the complete vendored file from
[C2SP Project Wycheproof](https://github.com/C2SP/wycheproof),
`testvectors_v1/aes_cmac_test.json`, version 0.9 (311 cases). Copyright Google
and contributors; **Apache License, Version 2.0**
(<https://www.apache.org/licenses/LICENSE-2.0>).

Each AES key-size test binary runs the 102 cases for that key size plus the 5
`InvalidKeySize` cases (fixed-length API rejects wrong key lengths).

These files are test inputs only and are not linked into embedded library builds.
