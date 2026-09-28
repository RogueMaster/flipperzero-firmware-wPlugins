<!-- SPDX-FileCopyrightText: Mistial Dev -->
<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# TLV parsing

Enable `TINY_CRYPTO_ENABLE_TLV=ON` and include `<tiny_crypto/tlv.h>`.
Select the encoding explicitly: DER, ISO 7816, or BER. BER additionally needs
`TINY_CRYPTO_TLV_BER=ON`.

`TC_TLV_read` reads one definite-length object without checking its children.
`TC_TLV_read_tree` checks constructed boundaries and also handles indefinite
BER lengths. Both return spans into the original buffer and stop before the
next sibling. Keep that buffer alive and unchanged while using the spans.

```c
#include <tiny_crypto/tlv.h>

TC_TLV_result read_ber_object(TC_bytes input, TC_TLV_element* object)
{
    enum { MAX_BYTES = 4096, MAX_ELEMENTS = 128, MAX_DEPTH = 8 };
    const TC_TLV_limits limits = {MAX_BYTES, MAX_BYTES, MAX_ELEMENTS, MAX_DEPTH};
    TC_TLV_frame frames[MAX_DEPTH];

    return TC_TLV_read_tree(input.data, input.length, TC_TLV_BER,
                           &limits, frames, MAX_DEPTH, object);
}
```

Use `object` only after `TC_TLV_OK`. `TC_TLV_MORE` means the object is truncated;
request more input or reject an incomplete message. `TC_TLV_INVALID` means bad
framing, and `TC_TLV_LIMIT` means a configured resource bound was exceeded.
Errors leave `object` unchanged. Frame scratch may change.

`object.encoded` includes the whole object, including its end-of-contents bytes
when present. `object.value` excludes the outer header and end-of-contents bytes.
For an indefinite object, use `value.length`, not `header.length`, to find its
content size. To require exactly one object, also check that `encoded.length`
equals the input length.

Use `TC_TLV_walk` for a whole tree or sequence of roots. It visits borrowed
primitive chunks and applies one element/depth budget across the input. The
incremental stream API provides the same traversal for fragmented input when
`TINY_CRYPTO_TLV_STREAM=ON` is enabled. Neither requires heap allocation.

These APIs check framing, not a schema. A DER-framed object can still contain
an invalid INTEGER, unordered SET, or missing certificate field. Use typed DER
and object parsers for those checks.
