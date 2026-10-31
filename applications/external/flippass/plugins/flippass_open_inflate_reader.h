#pragma once

#include "flippass_open_inflate_plugin.h"

typedef struct {
    const FlipPassOpenInflateHostApiV1* host_api;
    size_t skip_bytes;
    size_t remaining_bytes;
    bool failed;
    bool truncated;
} FlipPassOpenInflateReaderContext;

static size_t fp_open_payload_reader_read(void* out, size_t capacity, void* context) {
    FlipPassOpenInflateReaderContext* reader = context;
    size_t out_size = 0U;

    if(reader == NULL || out == NULL || capacity == 0U || reader->failed) {
        return 0U;
    }

    while(reader->skip_bytes > 0U) {
        const size_t discard_capacity = (reader->skip_bytes < capacity) ? reader->skip_bytes :
                                                                          capacity;
        size_t discarded = 0U;

        if(!reader->host_api->read_staged_payload_stream(
               reader->host_api->context, out, discard_capacity, &discarded)) {
            reader->failed = true;
            return 0U;
        }
        if(discarded == 0U) {
            reader->failed = true;
            reader->truncated = true;
            return 0U;
        }

        if(discarded > discard_capacity) {
            reader->failed = true;
            return 0U;
        }
        reader->skip_bytes -= discarded;
    }

    if(reader->remaining_bytes == 0U) {
        return 0U;
    }

    const size_t request = (reader->remaining_bytes < capacity) ? reader->remaining_bytes :
                                                                  capacity;
    if(!reader->host_api->read_staged_payload_stream(
           reader->host_api->context, out, request, &out_size)) {
        reader->failed = true;
        return 0U;
    }
    if(out_size > request || out_size > reader->remaining_bytes) {
        reader->failed = true;
        return 0U;
    }
    if(out_size == 0U) {
        reader->failed = true;
        reader->truncated = true;
        return 0U;
    }

    reader->remaining_bytes -= out_size;

    return out_size;
}
