#pragma once

#include "types.h"

/* Strip leading and trailing zero bits. Returns the inclusive [start, end)
 * range into `bits` of the inner content. If the result is empty, *out_start
 * and *out_len are both set to 0. */
void bits_strip_padding(
    const uint8_t* bits,
    uint16_t len,
    uint16_t* out_start,
    uint16_t* out_len);

/* Shannon entropy of a binary stream, normalized to [0,1] (max == 1.0). */
float bits_entropy(const uint8_t* bits, uint32_t len);

/* Fraction of zero bits in the stream. Empty stream returns 1.0. */
float bits_zero_ratio(const uint8_t* bits, uint32_t len);

/* Iterate run-length encoded segments by walking a buffer or virtual
 * concatenation. The callback receives (value, length) for each run.
 *
 * `iter_segments`: optional context for cross-segment iteration where the
 * caller stitches together segments without a single contiguous buffer.
 *
 * The callback MUST return true to continue, false to stop.
 */
typedef bool (*BitsRunCallback)(uint8_t value, uint16_t length, void* ctx);

/* Run-length-encode a contiguous bit buffer, invoking cb for each run. */
void bits_for_each_run(
    const uint8_t* bits,
    uint32_t len,
    BitsRunCallback cb,
    void* ctx);

/* Pack bits MSB-first into bytes. Output is a hex string with single
 * spaces between bytes. Writes at most cap-1 bytes plus null terminator.
 * Returns the number of bytes written (excluding null). */
size_t bits_to_hex(const uint8_t* bits, uint16_t len, char* out, size_t cap);

/* Detect the longest alternating run >= 8 bits. */
PreambleInfo bits_detect_preamble(const uint8_t* bits, uint16_t len);

/* Mean pairwise Hamming similarity of segments[1..n-1] vs segments[0],
 * scoped to the inner_len of each segment. Stores the result in *out_value.
 * Returns true if at least one pairwise comparison was possible. */
bool bits_segment_similarity(const SubFile* sub, float* out_value);

/* Find shortest period p in [min_len, max_len] where bits[:p] repeats
 * >= 2 times covering >= 50% of the stream. Returns 0 if not found.
 * Walks a virtual concatenation of inner segments via the SubFile. */
uint16_t bits_find_repeating_subpattern_inner(
    const SubFile* sub,
    uint16_t* out_reps);
