#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <lib/toolbox/bit_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* BER-TLV data objects, encoded per ITU-T X.690.
 *
 * Reads take a buffer and a length rather than a BitBuffer, so a caller can
 * walk a message header by header without copying, and so parsing can be
 * tested without a transport.
 *
 * Supports tags of one or two octets and definite lengths of up to two length
 * octets. Anything else is rejected.
 */

/* Two tag octets, a length form octet, and two length octets. */
#define SEOS_TLV_HEADER_MAX 5

/* Largest value a length can describe here. */
#define SEOS_TLV_LENGTH_MAX 0xffff

typedef struct {
    uint16_t tag;
    const uint8_t* value;
    size_t value_len;
    /* Position in the buffer the object was read from, for callers that work
     * in offsets rather than pointers. */
    size_t header_offset;
    size_t value_offset;
} SeosTlvObject;

typedef struct {
    const uint8_t* data;
    size_t len;
    size_t offset;
} SeosTlvCursor;

void seos_tlv_cursor_init(SeosTlvCursor* cursor, const uint8_t* data, size_t len);

/* Whether the cursor has reached the end of its buffer. */
bool seos_tlv_cursor_done(const SeosTlvCursor* cursor);

/* Reads the object at the cursor and advances past its value.
 *
 * Returns false and leaves the cursor unchanged if the object is malformed or
 * extends past the end of the buffer. */
bool seos_tlv_read(SeosTlvCursor* cursor, SeosTlvObject* out);

/* Reads the object at `offset` without a cursor. */
bool seos_tlv_read_at(const uint8_t* data, size_t len, size_t offset, SeosTlvObject* out);

/* Reads a bare tag and advances `offset` past it. A tag list holds tags with
 * no lengths between them. */
bool seos_tlv_read_tag(const uint8_t* data, size_t len, size_t* offset, uint16_t* tag);

/* Octets a length takes in its shortest form. */
size_t seos_tlv_length_size(size_t value_len);

/* Writes a tag and length into `out` and returns the number of bytes written.
 * `out` must have room for SEOS_TLV_HEADER_MAX. Uses the shortest legal
 * form. */
size_t seos_tlv_write_header(uint8_t* out, uint16_t tag, size_t value_len);

/* Appends a whole object to a buffer. */
void seos_tlv_append(BitBuffer* out, uint16_t tag, const uint8_t* value, size_t value_len);

#ifdef __cplusplus
}
#endif
