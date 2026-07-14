#pragma once
#include <furi.h>

enum {
    FlagEject = 1 << 5,
    FlagBackButtonPressed = 1 << 4,
};

typedef struct {
    FuriThreadId* thread_id;
    uint64_t card_size_in_blocks;
    uint32_t logical_block_size;

    uint64_t bytes_read;
    uint64_t bytes_written;
} Context;
