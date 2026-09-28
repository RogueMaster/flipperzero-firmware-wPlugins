<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# MD5 download checksums

Enable `TINY_CRYPTO_ENABLE_MD5=ON` and include `<tiny_crypto/md5.h>`.
The desktop profile enables it by default. CCL parsing can be built separately.
MD5 supports compatibility with published download checksums such as the TSA CCL.
Its broken collision resistance makes authenticated transport or trusted
provisioning essential when the download affects credential acceptance.

Use `TC_MD5_init`, `TC_MD5_update` and `TC_MD5_final` for chunked input, or
`TC_MD5_digest` for a complete buffer. Digests contain 16 bytes. Compare them to
the decoded expected checksum after receiving the complete file. Hash the exact
download bytes, including line endings.

Initialize a context before use and before reuse after finalization. Context
storage must be disjoint from input and output. One-shot input and output may
overlap. Argument failures preserve output and context. With `TC_ZEROIZE=1`,
finalization clears the context; `TC_MD5_ctx_clear` always clears it.

For C++11, include `<tiny_crypto/hash.hpp>` and use `tiny_crypto::MD5`.
It provides `update`, `finish`, `reset` and static `digest` methods with
pointer-length and C-array overloads. A successful `finish` resets the object
for another message. Destruction clears its context.

The implementation follows the [RFC 1321 algorithm](https://www.rfc-editor.org/rfc/rfc1321.html).
It shares 64-byte buffering and padding with the SHA-1/SHA-224/SHA-256 core.
Complete blocks are read directly from input; the context retains partial
blocks. Length encoding uses the low 64 bits of the bit count. AVR constant
tables use program memory when `TC_AVR_PROGMEM` is enabled.

`test_md5_0` and `test_md5_1` cover both zeroization settings, RFC known answers,
independent padding-boundary answers, every split of those inputs, a million-byte
message, invalid arguments and supported overlap. Run them with:

```sh
ctest --test-dir build --output-on-failure -R '^test_(cpp_)?md5_[01]$'
```

The [CCL external-file test](twic-ccl.md#tests) checks the downloaded list against
its separately supplied expected checksum.
