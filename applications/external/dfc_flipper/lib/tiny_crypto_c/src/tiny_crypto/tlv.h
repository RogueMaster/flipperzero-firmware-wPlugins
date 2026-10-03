/* SPDX-FileCopyrightText: Mistial Dev
 * SPDX-License-Identifier: GPL-2.0-or-later */
/* Bounded TLV readers for DER, ISO/IEC 7816-4 and ASN.1 BER: single objects,
 * sibling readers, tree checks, whole-tree walks and an incremental stream.
 * Standards: ITU-T X.690 (02/2021), ISO/IEC 7816-4:2020 sections 6.4 and
 * 8.1.2.
 * Configuration: TC_ENABLE_TLV, TC_TLV_ENABLE_BER and TC_TLV_ENABLE_STREAM.
 * Limitations: framing checks only. der.h and the object readers check
 * values and schemas.
 * Contracts: docs/api.md. Guide: docs/tlv.md. */
#ifndef TINY_CRYPTO_TLV_H_
#define TINY_CRYPTO_TLV_H_

#include <tiny_crypto/common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef TC_result TC_TLV_result;
#define TC_TLV_OK TC_RESULT_OK
#define TC_TLV_END TC_RESULT_END
#define TC_TLV_MORE TC_RESULT_MORE
#define TC_TLV_INVALID TC_RESULT_INVALID
#define TC_TLV_LIMIT TC_RESULT_LIMIT
#define TC_TLV_UNSUPPORTED TC_RESULT_UNSUPPORTED
#define TC_TLV_ARGUMENT TC_RESULT_ARGUMENT
#define TC_TLV_IO TC_RESULT_IO /* Backing storage could not supply requested bytes. */

typedef enum {
  TC_TLV_DER = 0,
  TC_TLV_ISO7816 = 1,
  TC_TLV_BER = 2,
  /* ISO/IEC 7816-4 section 8.1.2 padding bytes, accepted only between root
   * objects. Section 6.4 forbids padding inside a constructed template. */
  TC_TLV_ISO7816_PAD_ZERO = 3,
  TC_TLV_ISO7816_PAD_ZERO_FF = 4
} TC_TLV_profile;

typedef struct {
  /* All limits are inclusive. Zero allows no bytes/elements/constructed
   * levels, not unlimited input. Indefinite values exclude their own EOC. */
  size_t max_input, max_value, max_elements, max_depth;
} TC_TLV_limits;

/* Up to a 32-bit ASN.1 tag number. Raw tag bytes retain class and encoding:
 * the on-card tag 7F21 has ASN.1 tag number 0x21.
 * ISO 7816 limits tags to three bytes. ASN.1 can use up to six here. */
#define TC_TLV_TAG_BYTES 6
#define TC_TLV_HEADER_BYTES (TC_TLV_TAG_BYTES + 1 + sizeof(size_t))
typedef struct {
  uint32_t number;
  size_t length;
  uint8_t tag[TC_TLV_TAG_BYTES];
  uint8_t tag_length, header_length, tag_class, constructed, indefinite;
} TC_TLV_header;

typedef struct {
  TC_TLV_header header;
  TC_bytes encoded, value;
} TC_TLV_element;

/* Shared rules. input borrows the encoded bytes. NULL data is valid only for
 * an empty span. Input must remain alive and unchanged while any returned
 * span is used. Input, parser state, frame storage and output structs must
 * not overlap. The readers charge no work. Limits bound them. Results:
 *
 *   OK           the item was read and outputs are written.
 *   END          no more siblings (readers only).
 *   MORE         the input ends inside an object. At the end of a complete
 *                message it means truncation.
 *   INVALID      bad framing: a zero first tag byte, an FF first tag byte
 *                under ISO 7816, a non-minimal tag number, universal tag 0
 *                outside BER EOC, an FF length byte, an indefinite length
 *                outside BER constructed objects, a non-minimal DER length,
 *                or a value that overruns its parent.
 *   LIMIT        a tag longer than TC_TLV_TAG_BYTES (three under ISO 7816),
 *                more length octets than a size_t holds (four under ISO
 *                7816), a value above max_value, input above max_input, or
 *                too many elements, levels or frames.
 *   UNSUPPORTED  TC_TLV_BER in a build without TC_TLV_ENABLE_BER, or an
 *                indefinite length passed to TC_TLV_read.
 *   ARGUMENT     NULL limits, reader or output, NULL input data with a
 *                length, an unknown profile, or a checked overlap.
 *
 * Failure leaves every output unchanged. DER here checks framing only, with
 * X.690 section 10.1 definite minimal lengths. Typed and schema checks
 * belong to der.h and the object readers. */

#if TC_ENABLE_TLV
/* Read only the tag and length octets at the start of input. */
TC_TLV_result TC_TLV_header_read(TC_bytes input, TC_TLV_profile profile,
                                 const TC_TLV_limits* limits, TC_TLV_header* out);
/* Read one definite-length element at the start of input and return its
 * encoding and value. Following bytes are left unread. MORE means the value
 * extends past input. An indefinite BER length returns UNSUPPORTED. Use
 * TC_TLV_read_tree, the walk or the stream for indefinite BER. */
TC_TLV_result TC_TLV_read(TC_bytes input, TC_TLV_profile profile, const TC_TLV_limits* limits,
                          TC_TLV_element* out);
#endif

/* Sibling cursor over borrowed input. Treat members as read-only and start it
 * with TC_TLV_reader_init or TC_TLV_reader_child. root is set by
 * TC_TLV_reader_init and cleared by TC_TLV_reader_child. Only a root reader
 * skips padding for the padded ISO 7816 profiles. */
typedef struct {
  TC_bytes input;
  TC_TLV_limits limits;
  size_t offset, elements;
  TC_TLV_profile profile;
  uint8_t root;
} TC_TLV_reader;
#if TC_ENABLE_TLV
/* Start a root reader over a complete data field or payload. Limits are copied.
 * input is borrowed for the reader's lifetime.
 * Returns ARGUMENT for NULL reader/limits, NULL input data with a length, a
 * reader that overlaps input or an unknown profile, UNSUPPORTED for BER when
 * disabled and LIMIT when input.length exceeds max_input. Failure leaves
 * reader unchanged. */
TC_TLV_result TC_TLV_reader_init(TC_TLV_reader* reader, TC_bytes input, TC_TLV_profile profile,
                                 const TC_TLV_limits* limits);
/* Start a reader over the template of an element read from parent. The child
 * inherits the parent's profile and limits, starts its own element count and
 * rejects padding. Its template is complete, so a truncated nested element
 * returns INVALID. The element value must lie inside the parent input.
 * Returns ARGUMENT for NULL pointers or an element outside the parent, and
 * leaves child unchanged on failure. child may reuse the parent's storage. */
TC_TLV_result TC_TLV_reader_child(TC_TLV_reader* child, const TC_TLV_reader* parent,
                                  const TC_TLV_element* element);
/* Read the next sibling. END means no more siblings and moves the reader
 * past trailing root padding. MORE means a root reader's next element is
 * truncated, and the reader stays in place for a retry with more input.
 * INVALID covers bad framing, and padding or truncation in a child reader.
 * LIMIT means max_elements or max_value was exceeded. ARGUMENT reports a
 * NULL pointer or a corrupted reader. Failure leaves reader and out
 * unchanged. Each call reads one level. Use walk to enforce a shared budget
 * across an entire tree. */
TC_TLV_result TC_TLV_next(TC_TLV_reader* reader, TC_TLV_element* out);
#endif

typedef enum { TC_TLV_BEGIN, TC_TLV_VALUE, TC_TLV_CLOSE } TC_TLV_event_kind;
typedef struct {
  TC_TLV_event_kind kind;
  size_t offset, depth;
  /* Header is populated for BEGIN only. bytes contains the exact header.
   * VALUE borrows a chunk of primitive content. CLOSE bytes is empty for
   * definite objects, or the two EOC bytes for indefinite objects.
   * TC_TLV_walk spans borrow its input, with bytes.data at input + offset.
   * A stream span borrows the fed chunk, except a header or EOC split across
   * chunks, which borrows stream storage for the callback's duration only. */
  TC_TLV_header header;
  TC_bytes bytes;
} TC_TLV_event;
typedef void (*TC_TLV_visit)(void* user, const TC_TLV_event* event);

typedef struct {
  size_t end, bound, start;
  uint8_t indefinite, resource_bound;
} TC_TLV_frame;

/* Caller-owned nesting storage for one decode: capacity frames at data. A
 * decode needs one frame per open constructed level, so capacity equals the
 * deepest nesting accepted. Fewer frames return LIMIT. The frames are
 * scratch, may change on failure and must not overlap the input. */
typedef struct {
  TC_TLV_frame* data;
  size_t capacity;
} TC_TLV_frames;

/* Treat members as private after init. Frames are caller-owned and must not
 * alias input or the stream. Callbacks must not modify/reenter the parser.
 * Wait for finish to succeed before acting on events. */
typedef struct {
  TC_TLV_limits limits;
  TC_TLV_frame* frames;
  size_t capacity, depth, offset, elements, remaining;
  TC_TLV_profile profile;
  TC_TLV_result error;
  uint8_t header[TC_TLV_HEADER_BYTES];
  uint8_t used, primitive, finished;
} TC_TLV_stream;

#if TC_ENABLE_TLV && TC_TLV_ENABLE_STREAM
/* Start an incremental decode of a sequence of root objects. Limits are
 * copied. frames is borrowed until the decode ends. ARGUMENT for a NULL
 * stream or NULL frames with a capacity, plus the shared checks. */
TC_TLV_result TC_TLV_stream_init(TC_TLV_stream* stream, TC_TLV_profile profile,
                                 const TC_TLV_limits* limits, TC_TLV_frames frames);
/* Consume a chunk without retaining its address and emit events to visit,
 * which may be NULL. Callbacks borrow spans only for their duration. OK and
 * MORE both consume the entire chunk. MORE means an object is unfinished.
 * An error found after the argument checks is sticky. Every later feed and
 * finish returns it until init. Events emitted before an error remain emitted, so
 * discard the message on error. Input beyond max_input returns LIMIT. A feed
 * after a successful finish returns ARGUMENT. */
TC_TLV_result TC_TLV_stream_feed(TC_TLV_stream* stream, TC_bytes chunk, TC_TLV_visit visit,
                                 void* user);
/* End the message. OK when every object is complete. INVALID for an
 * unfinished header, value or constructed object, sticky like feed. A stored
 * feed error is returned unchanged. */
TC_TLV_result TC_TLV_stream_finish(TC_TLV_stream* stream);
#endif
#if TC_ENABLE_TLV
/* Walk checks all constructed boundaries with one shared element/depth budget.
 * Event spans borrow input and stay valid while input is alive and unchanged.
 * A sequence of root objects is accepted. A schema needing exactly one root
 * must check that separately. NULL visit validates framing without callbacks.
 * Input is complete, so truncation returns INVALID. Results follow the stream
 * feed and finish rules. */
TC_TLV_result TC_TLV_walk(TC_bytes input, TC_TLV_profile profile, const TC_TLV_limits* limits,
                          TC_TLV_frames frames, TC_TLV_visit visit, void* user);

/* Read one complete object and validate its constructed boundaries with one
 * shared element and depth budget. Supports indefinite BER and leaves
 * following siblings unread. Returned spans borrow input. encoded includes
 * EOC and value excludes it. MORE means truncation. Frames may change on
 * failure. out changes only on OK. Input, limits, frames and out must be
 * disjoint. Root padding after the object is left unread. */
TC_TLV_result TC_TLV_read_tree(TC_bytes input, TC_TLV_profile profile, const TC_TLV_limits* limits,
                               TC_TLV_frames frames, TC_TLV_element* out);
#endif

#ifdef __cplusplus
}
#endif
#endif
