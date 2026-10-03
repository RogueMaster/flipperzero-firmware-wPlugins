<!-- SPDX-FileCopyrightText: Mistial Dev -->

<!-- SPDX-License-Identifier: GPL-2.0-or-later -->

# TLV parsing

Enable `TINY_CRYPTO_ENABLE_TLV=ON` and include `<tiny_crypto/tlv.h>`.
Select the encoding explicitly: DER, ISO 7816, or BER. BER additionally needs
`TINY_CRYPTO_TLV_ENABLE_BER=ON`.

`TC_TLV_read` reads one definite-length object without checking its children.
`TC_TLV_read_tree` checks constructed boundaries and also handles indefinite
BER lengths. Both return spans into the original buffer and stop before the
next sibling. Keep that buffer alive and unchanged while using the spans.
Every TLV and DER entry point takes its input as a borrowed `TC_bytes` span.
A span with NULL data and a nonzero length returns `TC_TLV_ARGUMENT`.

```c
#include <tiny_crypto/tlv.h>

TC_TLV_result read_ber_object(TC_bytes input, TC_TLV_element* object)
{
    enum { MAX_BYTES = 4096, MAX_ELEMENTS = 128, MAX_DEPTH = 8 };
    const TC_TLV_limits limits = {MAX_BYTES, MAX_BYTES, MAX_ELEMENTS, MAX_DEPTH};
    TC_TLV_frame frames[MAX_DEPTH];

    return TC_TLV_read_tree(input, TC_TLV_BER, &limits,
                            (TC_TLV_frames){frames, MAX_DEPTH}, object);
}
```

Every decoder that validates nesting takes its frame storage as
`TC_TLV_frames`, a caller-owned array and its capacity in frames. It needs one
frame per constructed nesting level, so `limits.max_depth` frames always
suffice. The frames are scratch and may change on failure.

Use `object` only after `TC_TLV_OK`. `TC_TLV_MORE` means the object is truncated.
Request more input or reject an incomplete message. `TC_TLV_INVALID` means bad
framing, and `TC_TLV_LIMIT` means a configured resource bound was exceeded.
Errors leave `object` unchanged. Frame scratch may change.

`object.encoded` includes the whole object, including its end-of-contents bytes
when present. `object.value` excludes the outer header and end-of-contents bytes.
For an indefinite object, take its content size from `value.length`.
`header.length` holds the encoded length field, which the indefinite form
omits. To require exactly one object, also check that `encoded.length` equals
the input length.

## Sibling readers

`TC_TLV_reader_init` starts a root reader over a complete data field or
payload. `TC_TLV_next` returns one sibling per call and leaves its value
unread. To read the template of a constructed element, open a child reader with
`TC_TLV_reader_child`. The child borrows the element value, inherits the
parent's profile and limits, and starts its own element count.

The padded ISO 7816 profiles skip `00` (and `FF` for
`TC_TLV_ISO7816_PAD_ZERO_FF`) only in a root reader. ISO/IEC 7816-4:2020
sections 8.1.2 and 8.1.3 permit padding between root data objects, and section 6.4
requires a constructed template to hold nested data objects without padding. A
child reader therefore returns `TC_TLV_INVALID` for a padding byte. A child
template is a complete value, so a truncated nested element also returns
`TC_TLV_INVALID`. A reader started with `TC_TLV_reader_init` on a value span is
a root reader, so use `TC_TLV_reader_child` for nested templates. Start every
reader with one of these two functions.

```c
#include <tiny_crypto/tlv.h>

/* Count the data objects inside each template of a padded response. */
TC_TLV_result count_nested(TC_bytes response, size_t* nested)
{
    enum { MAX_BYTES = 1024, MAX_ELEMENTS = 64, MAX_DEPTH = 4 };
    const TC_TLV_limits limits = {MAX_BYTES, MAX_BYTES, MAX_ELEMENTS, MAX_DEPTH};
    TC_TLV_reader root, child;
    TC_TLV_element object, inner;
    TC_TLV_result result;
    size_t count = 0;

    result = TC_TLV_reader_init(&root, response, TC_TLV_ISO7816_PAD_ZERO_FF,
                                &limits);
    while (result == TC_TLV_OK &&
           (result = TC_TLV_next(&root, &object)) == TC_TLV_OK) {
        if (!object.header.constructed)
            continue;
        result = TC_TLV_reader_child(&child, &root, &object);
        while (result == TC_TLV_OK &&
               (result = TC_TLV_next(&child, &inner)) == TC_TLV_OK)
            ++count;
        if (result == TC_TLV_END)
            result = TC_TLV_OK;
    }
    if (result != TC_TLV_END)
        return result; /* MORE here means a truncated response. */
    *nested = count;
    return TC_TLV_OK;
}
```

`TC_TLV_next` returns `TC_TLV_END` when no siblings remain. A root reader
returns `TC_TLV_MORE` when the next element is truncated. Failures leave the
reader and element unchanged.

## Whole-tree traversal

Use `TC_TLV_walk` for a whole tree or sequence of roots. It visits borrowed
primitive chunks and applies one element/depth budget across the input. The
incremental stream API provides the same traversal for fragmented input when
`TINY_CRYPTO_TLV_ENABLE_STREAM=ON` is enabled. Neither requires heap allocation.

Every `TC_TLV_walk` event span points into the walked input at the event
offset, so a visitor may keep BEGIN headers, VALUE chunks and EOC markers while
that input stays alive and unchanged. Stream events borrow the fed chunk. A
header or EOC split across two chunks is delivered from stream storage and is
valid only during the callback.

These APIs check framing only. A DER-framed object can still contain
an invalid INTEGER, unordered SET, or missing certificate field. Use the typed
[DER readers](der.md) and object parsers for those checks.
