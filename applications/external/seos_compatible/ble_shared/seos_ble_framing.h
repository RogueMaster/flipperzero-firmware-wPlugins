#pragma once

#include "../seos_ble_plugin.h"

#include <lib/toolbox/bit_buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Splitting a message across BLE frames, and putting it back together.
 *
 * The framing is the credential protocol's own, not the radio's: a flag byte
 * saying where the chunk sits, then up to BLE_CHUNK_SIZE bytes. Both stacks
 * speak it, so both share this.
 */

typedef enum {
    SeosBleFrameIncomplete, /* more chunks to come */
    SeosBleFrameComplete, /* `rx` now holds the whole message */
    SeosBleFrameError, /* the frame is not one we can use */
} SeosBleFrameResult;

/* Adds one received frame to the message being assembled in `rx`. */
SeosBleFrameResult seos_ble_reassemble(BitBuffer* rx, const uint8_t* frame, size_t len);

/* Hands one chunk to the transport. Returning false stops the rest. */
typedef bool (*SeosBleChunkFn)(void* context, const uint8_t* chunk, size_t chunk_len);

/* Splits `data` into chunks and passes each to `send`, in order. Returns
 * false if `send` refused one, leaving the remainder unsent. */
bool seos_ble_chunk(const uint8_t* data, size_t size, SeosBleChunkFn send, void* context);

#ifdef __cplusplus
}
#endif
