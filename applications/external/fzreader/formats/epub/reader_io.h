#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Random access to a byte source, so the ZIP reader can be tested without a Flipper. */
typedef struct {
    /** Read exactly size bytes at offset. @returns false on short read or I/O error. */
    bool (*read_at)(void* context, uint64_t offset, void* buffer, size_t size);
    /** Total size in bytes, 0 if unknown. */
    uint64_t (*size)(void* context);
    void* context;
} ReaderIo;
