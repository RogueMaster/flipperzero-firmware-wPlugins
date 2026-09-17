<!--
SPDX-FileCopyrightText: Mistial Dev
SPDX-License-Identifier: Unlicense
-->

# AES-SIV test vectors

## RFC 5297 Appendix A

The deterministic and nonce-based examples in `siv_test.c` are transcribed from
[RFC 5297](https://datatracker.ietf.org/doc/html/rfc5297) Appendix A (SIV-AES
with AES-SIV-CMAC-256). RFC 5297 is Informational; the vectors are used only for
interoperability testing.

## Wycheproof

`aead_aes_siv_cmac_test.json` is vendored from
[C2SP Project Wycheproof](https://github.com/C2SP/wycheproof),
file `testvectors_v1/aead_aes_siv_cmac_test.json`, version 0.9. Copyright Google
and contributors; **Apache License, Version 2.0**
(<https://www.apache.org/licenses/LICENSE-2.0>).

Wycheproof AEAD cases map to RFC 5297 S2V as associated-data vector
`(aad, nonce)` then plaintext (empty `aad` is still included as an empty
component; the nonce is the final AD string).

These files are test inputs only and are not linked into embedded library builds.
