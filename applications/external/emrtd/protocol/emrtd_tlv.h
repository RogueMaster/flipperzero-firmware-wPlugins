/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * BER-TLV reading, ISO/IEC 8825-1.
 *
 * Everything inside a passport - the LDS files, the security infos, the
 * Secure Messaging envelopes - is nested tag-length-value. The reader has to
 * walk kilobytes of it on a device with 186 KB of heap, so this
 * parser never copies and never allocates: a node points into the buffer it
 * was parsed from, and iteration is a cursor.
 *
 * It is also the layer most exposed to a malformed or hostile chip, so every
 * function is written to stay inside the buffer it was handed and to fail
 * rather than guess.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One node, pointing into the buffer it was parsed from. */
typedef struct {
    uint32_t tag; /**< Up to four tag bytes, packed big endian, e.g. 0x5F1F. */
    const uint8_t* value;
    size_t value_len;
    bool constructed; /**< Bit 6 of the first tag byte. */
    const uint8_t* start; /**< First byte of the whole node, tag included. */
    size_t total_len; /**< Tag plus length plus value. */
} EmrtdTlv;

/** A cursor over a sequence of sibling nodes. */
typedef struct {
    const uint8_t* data;
    size_t len;
    size_t offset;
} EmrtdTlvIter;

/** Start iterating over a buffer of concatenated TLV nodes. */
void emrtd_tlv_iter_init(EmrtdTlvIter* iter, const uint8_t* data, size_t len);

/** Start iterating over the children of a constructed node. */
void emrtd_tlv_iter_children(EmrtdTlvIter* iter, const EmrtdTlv* node);

/**
 * Advance to the next node.
 *
 * @return false at the end of the buffer or on a malformed node; the two are
 *         told apart with emrtd_tlv_iter_exhausted().
 */
bool emrtd_tlv_iter_next(EmrtdTlvIter* iter, EmrtdTlv* out);

/** True when iteration stopped because the buffer ran out, not because of an error. */
bool emrtd_tlv_iter_exhausted(const EmrtdTlvIter* iter);

/** Parse the single node at the head of a buffer. */
bool emrtd_tlv_parse_first(const uint8_t* data, size_t len, EmrtdTlv* out);

/** Find a direct child by tag. */
bool emrtd_tlv_find(const uint8_t* data, size_t len, uint32_t tag, EmrtdTlv* out);

/** Find a node by tag anywhere in the subtree, depth first. */
bool emrtd_tlv_find_recursive(const uint8_t* data, size_t len, uint32_t tag, EmrtdTlv* out);

/**
 * Total length of the node whose header begins a buffer.
 *
 * Used to size a file read from its first few bytes, before the rest has been
 * fetched. Returns 0 when the header is still incomplete.
 */
size_t emrtd_tlv_total_length(const uint8_t* header, size_t len);

/** Read an unsigned integer value of up to four bytes. */
bool emrtd_tlv_read_uint(const EmrtdTlv* node, uint32_t* out);

/** Format a tag for display, e.g. "5F1F". Returns @p out. */
const char* emrtd_tlv_tag_str(uint32_t tag, char* out, size_t out_size);

/** Encode a BER length. Returns the number of bytes written, at most four. */
size_t emrtd_tlv_encode_length(size_t length, uint8_t* out);

/** Bytes a BER length field will occupy. */
size_t emrtd_tlv_encoded_length_size(size_t length);

#ifdef __cplusplus
}
#endif
