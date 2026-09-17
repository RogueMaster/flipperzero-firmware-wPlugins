/* Flipper implementation of the engine's response buffer.
 *
 * The radio stack has its own BitBuffer. The engine writes here first and the
 * listener copies the finished response across, so neither type leaks into the
 * other.
 */
#include "dfc_bytebuf.h"
#include "dfc_port.h"

#include <string.h>

#include <furi.h>

DfcByteBuf* dfc_bytebuf_alloc(size_t max_size) {
    DFC_UNUSED(max_size);
    DfcByteBuf* buffer = malloc(sizeof(DfcByteBuf));
    if(buffer) memset(buffer, 0, sizeof(DfcByteBuf));
    return buffer;
}

void dfc_bytebuf_free(DfcByteBuf* b) {
    free(b);
}

void dfc_bytebuf_reset(DfcByteBuf* b) {
    b->size_bytes = 0;
}

void dfc_bytebuf_append_bytes(DfcByteBuf* b, const uint8_t* data, size_t len) {
    if(b->size_bytes + len > DFC_BYTEBUF_MAX) return;
    memcpy(b->data + b->size_bytes, data, len);
    b->size_bytes += len;
}

void dfc_bytebuf_append_byte(DfcByteBuf* b, uint8_t byte) {
    if(b->size_bytes + 1 > DFC_BYTEBUF_MAX) return;
    b->data[b->size_bytes++] = byte;
}

size_t dfc_bytebuf_get_size_bytes(const DfcByteBuf* b) {
    return b->size_bytes;
}

const uint8_t* dfc_bytebuf_get_data(const DfcByteBuf* b) {
    return b->data;
}

uint8_t dfc_bytebuf_get_byte(const DfcByteBuf* b, size_t index) {
    return b->data[index];
}
