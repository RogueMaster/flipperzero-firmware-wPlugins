<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# GZIP decoding

Enable `TINY_CRYPTO_ENABLE_GZIP` and include `<tiny_crypto/gzip.h>`.
Direct-source builds use `TC_ENABLE_GZIP=1`. The desktop resource profile enables
it by default. GZIP decoding can be built independently of the cryptographic
algorithms and certificate parsers.
The `piv-acu` and `piv-pd` targets enable it across all resource profiles to
handle compressed card certificates.

`TC_GZIP_decode` takes a complete input buffer, output storage, a
`TC_GZIP_workspace`, a remaining work budget and an output-length pointer.
These storage regions must be separate. Workspace needs no initialization and
can be reused after each call. Use `sizeof(TC_GZIP_workspace)` when sizing it.

```c
enum { CERTIFICATE_CAPACITY = 4096, DECODE_WORK_LIMIT = 100000 };
uint8_t certificate[CERTIFICATE_CAPACITY];
TC_GZIP_workspace workspace;
size_t work = DECODE_WORK_LIMIT;
size_t certificate_length;

TC_GZIP_result result = TC_GZIP_decode(compressed, compressed_length,
    certificate, sizeof certificate, &workspace, &work, &certificate_length);
if (result == TC_GZIP_OK) {
    /* Parse the complete certificate, then verify its signature and trust. */
}
TC_secure_zero(certificate, sizeof certificate);
```

Choose capacity and budget for the application's accepted objects. Work counts
bounded decoding operations, including input bits, Huffman table entries,
expanded bytes and checksum bytes. The count is deterministic across processor
speeds. Cleanup still wipes the caller's output
capacity after a processing failure.

`TC_GZIP_LIMIT` reports exhausted output capacity or work. `TC_GZIP_INVALID`
reports malformed framing, compressed data or checksum/size mismatches.
`TC_GZIP_UNSUPPORTED` reports an unsupported compression method.
`TC_GZIP_ARGUMENT` reports invalid pointers or overlapping storage and preserves
all storage. Other failures clear the output buffer and preserve its length
parameter. Workspace is cleared after processing. A successful call writes the
decoded length and leaves bytes beyond that length unchanged.

The decoder supports stored, fixed-Huffman and dynamic-Huffman DEFLATE blocks,
optional GZIP headers and concatenated members. Each member gets its own history,
CRC32 and size checks; output capacity and work apply to the whole input.
Trailing non-member bytes fail. Decoded output doubles as back-reference history.
All storage is caller-owned.

In C++11, include `<tiny_crypto/gzip.hpp>` and use `tiny_crypto::GZIPDecoder`.
It owns one reusable workspace. `decode` takes the same input/output buffers as
the C function, with work and output length passed by reference:

```cpp
tiny_crypto::GZIPDecoder decoder;
uint8_t decoded[4096];
size_t work = 100000, length;
TC_GZIP_result result = decoder.decode(compressed, compressed_length,
    decoded, sizeof decoded, work, length);
```

For C arrays, `decoder.decode(compressed, decoded, work, length)` infers both sizes.

Keep input, output, decoder storage, work and length disjoint. Each call consumes
the supplied work budget; refill it before starting another independent operation.

For PIV certificate containers, `TC_PIV_certificate_read` identifies compressed
certificate bytes through `TC_PIV_CERTIFICATE_GZIP`. After decompression, require
the X.509 parser to consume the entire result. GZIP checksums detect accidental
corruption; credential authentication requires signature and trust
validation.

Run the focused tests with:

```sh
ctest --test-dir build -R '^test_(inflate|gzip_differential|gzip_corpus)$' --output-on-failure
```

The differential test generates synthetic inputs with Python's zlib across
compression levels and strategies. It checks decoded bytes, corrupted checksums,
concatenated members and insufficient output capacity.

With `TINY_CRYPTO_TLV_CORPUS` set to `tests/vectors`, `test_gzip_corpus` also
checks the SD33 compressed certificate objects against Python's decoder and
rejects checksum mutations. See [Testing](testing.md#fuzzing) for the bounded
decoder fuzz harness.

`test_piv_certificate_corpus` checks the corresponding certificate containers,
including explicit PIV/TWIC profile mismatches and extra fields. It requires the
same corpus setting and runs independently of decompression.
