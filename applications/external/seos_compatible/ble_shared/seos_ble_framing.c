#include "seos_ble_framing.h"

#include <furi.h>

#define TAG "SeosBleFraming"

SeosBleFrameResult seos_ble_reassemble(BitBuffer* rx, const uint8_t* frame, size_t len) {
    furi_assert(rx);

    if(len < 1) {
        FURI_LOG_W(TAG, "Empty frame");
        return SeosBleFrameError;
    }

    uint8_t flags = frame[0];
    if((flags & BLE_FLAG_ERR) == BLE_FLAG_ERR) {
        seos_log_buffer(TAG, "Received error response", (uint8_t*)frame + 1, len - 1);
        return SeosBleFrameError;
    }

    if((flags & BLE_FLAG_SOM) == BLE_FLAG_SOM) {
        bit_buffer_reset(rx);
    } else if(bit_buffer_get_size_bytes(rx) == 0) {
        FURI_LOG_W(TAG, "Expected start of BLE packet");
        return SeosBleFrameError;
    }

    if(bit_buffer_get_size_bytes(rx) + len - 1 > bit_buffer_get_capacity_bytes(rx)) {
        FURI_LOG_W(TAG, "Message longer than the buffer holding it");
        bit_buffer_reset(rx);
        return SeosBleFrameError;
    }
    bit_buffer_append_bytes(rx, frame + 1, len - 1);

    return (flags & BLE_FLAG_EOM) == BLE_FLAG_EOM ? SeosBleFrameComplete : SeosBleFrameIncomplete;
}

bool seos_ble_chunk(const uint8_t* data, size_t size, SeosBleChunkFn send, void* context) {
    furi_assert(send);
    if(size == 0) return true;

    size_t num_chunks = (size + BLE_CHUNK_SIZE - 1) / BLE_CHUNK_SIZE;
    uint8_t chunk[1 + BLE_CHUNK_SIZE];

    for(size_t i = 0; i < num_chunks; i++) {
        uint8_t flags = 0;
        if(i == 0) flags |= BLE_FLAG_SOM;
        if(i == num_chunks - 1) flags |= BLE_FLAG_EOM;
        /* The low nibble carries how many chunks are still to come. */
        flags |= (uint8_t)((num_chunks - 1 - i) & 0x0F);

        size_t remaining = size - (i * BLE_CHUNK_SIZE);
        size_t chunk_size = remaining > BLE_CHUNK_SIZE ? BLE_CHUNK_SIZE : remaining;

        chunk[0] = flags;
        memcpy(chunk + 1, data + (i * BLE_CHUNK_SIZE), chunk_size);
        if(!send(context, chunk, 1 + chunk_size)) return false;
    }

    return true;
}
