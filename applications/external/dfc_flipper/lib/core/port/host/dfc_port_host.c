/* Host implementation of the platform interface.
 *
 * Randomness is replayable so a recorded session reproduces exactly.
 * Allocation is the C library heap, because a host has one.
 */
#include "dfc_port.h"

#include <stdlib.h>
#include <string.h>

#include "dfc_port_host.h"

static uint8_t queue[DFC_HOST_RANDOM_BUFFER_MAX];
static size_t queue_len;
static size_t queue_offset;
static bool queue_set;

void dfc_host_clear_random_buffer(void) {
    queue_len = 0;
    queue_offset = 0;
    queue_set = false;
}

void dfc_host_set_random_buffer(const uint8_t* data, size_t len) {
    // Injections accumulate: a vector may queue the octets for one exchange
    // while the octets for an earlier one are still unread.
    size_t remaining = queue_len - queue_offset;
    if(queue_offset > 0 && remaining > 0) {
        memmove(queue, queue + queue_offset, remaining);
    }
    queue_offset = 0;
    queue_len = remaining;
    size_t room = sizeof(queue) - remaining;
    if(len > room) len = room;
    memcpy(queue + remaining, data, len);
    queue_len += len;
    queue_set = true;
}

size_t dfc_host_random_remaining(void) {
    return queue_len - queue_offset;
}

void dfc_random_fill(uint8_t* buf, size_t len) {
    if(queue_set) {
        size_t remaining = queue_len - queue_offset;
        size_t copy_len = len < remaining ? len : remaining;
        memcpy(buf, queue + queue_offset, copy_len);
        queue_offset += copy_len;
        if(copy_len < len) memset(buf + copy_len, 0, len - copy_len);
        if(queue_offset == queue_len) dfc_host_clear_random_buffer();
        return;
    }

    // No queue: a deterministic ramp, so an unscripted session is still
    // reproducible.
    for(size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)i;
    }
}

void* dfc_platform_alloc(size_t size, DfcAllocTag tag) {
    DFC_UNUSED(tag);
    void* block = malloc(size);
    if(block) memset(block, 0, size);
    return block;
}

void dfc_platform_free(void* ptr) {
    free(ptr);
}

void dfc_port_notify(void* context, DfcEvent event) {
    DFC_UNUSED(context);
    DFC_UNUSED(event);
}
