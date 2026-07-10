#pragma once

#include <stddef.h>
#include <stdint.h>

/* Pure, host-testable best-distance record codec: serialize/parse + update.
 *
 * A record is the best (highest) distance a player has achieved. It is persisted as a
 * fixed-size little-endian byte buffer with a magic/version marker for corruption detection.
 * The record is non-negative (int32_t >= 0). File I/O is handled elsewhere (Task 9);
 * this module is pure serialization/parsing + keeping the maximum.
 */

/* Fixed serialized size: 1 magic byte + 1 version byte + 2 reserved bytes + 4-byte
 * little-endian distance = 8 bytes total. Allows future extension without breaking
 * existing buffers. */
#define RECORD_BYTES 8

/* Serializes a best distance into a fixed 8-byte buffer in little-endian format.
 * Includes a magic/version tag (byte 0: 0x42='B', byte 1: 0x01=version 1).
 *
 * Returns: number of bytes written (RECORD_BYTES on success, 0 if out_len < RECORD_BYTES).
 * Writes nothing and returns 0 if the output buffer is too small.
 *
 * The buffer layout is:
 *   Byte 0: magic marker (0x42)
 *   Byte 1: version (0x01)
 *   Bytes 2-3: reserved (set to 0x00)
 *   Bytes 4-7: distance as little-endian int32_t
 */
size_t record_serialize(int32_t best, uint8_t *out, size_t out_len);

/* Parses a best distance from a fixed 8-byte buffer in little-endian format.
 * Validates the magic/version tag to detect corrupt or foreign data.
 *
 * Returns: the stored best distance on success, or 0 if:
 *   - len < RECORD_BYTES (buffer too short),
 *   - the magic byte (byte 0) is not 0x42, or
 *   - the version byte (byte 1) is not 0x01.
 *
 * Never reads past the first RECORD_BYTES bytes of buf, even if len is larger.
 * Never crashes on NULL or malformed input; returns 0 (safe default).
 * Note: cannot distinguish "0 distance" from "corrupt data", so both return 0.
 */
int32_t record_parse(const uint8_t *buf, size_t len);

/* Keeps the maximum of two distances. Used to update the record after a run.
 *
 * Returns: max(prev_best, distance). Clamps negative distances to prev_best
 * (a negative distance never lowers the record).
 *
 * Pure logic: no side effects, no globals.
 */
int32_t record_update(int32_t prev_best, int32_t distance);
