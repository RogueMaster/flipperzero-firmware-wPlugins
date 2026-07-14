#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef SEADER_HOST_TEST
#include "lib/host_tests/bit_buffer.h"
#else
#include <lib/toolbox/bit_buffer.h>
#endif

typedef struct {
    BitBuffer* tx;
    BitBuffer* rx;
    size_t tx_capacity;
    size_t rx_capacity;
} SeaderHfBufferPair;

bool seader_hf_buffer_pair_prepare(
    SeaderHfBufferPair* pair,
    size_t tx_capacity,
    size_t rx_capacity,
    size_t required_tx_len);
void seader_hf_buffer_pair_free(SeaderHfBufferPair* pair);
