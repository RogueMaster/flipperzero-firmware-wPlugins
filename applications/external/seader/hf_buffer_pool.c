#include "hf_buffer_pool.h"

static void seader_hf_buffer_pair_release_buffers(SeaderHfBufferPair* pair) {
    if(pair->tx) {
        bit_buffer_free(pair->tx);
        pair->tx = NULL;
    }
    if(pair->rx) {
        bit_buffer_free(pair->rx);
        pair->rx = NULL;
    }
    pair->tx_capacity = 0U;
    pair->rx_capacity = 0U;
}

bool seader_hf_buffer_pair_prepare(
    SeaderHfBufferPair* pair,
    size_t tx_capacity,
    size_t rx_capacity,
    size_t required_tx_len) {
    if(!pair || required_tx_len > tx_capacity || tx_capacity == 0U || rx_capacity == 0U) {
        return false;
    }

    if(pair->tx && pair->rx && pair->tx_capacity == tx_capacity &&
       pair->rx_capacity == rx_capacity) {
        bit_buffer_reset(pair->tx);
        bit_buffer_reset(pair->rx);
        return true;
    }

    seader_hf_buffer_pair_release_buffers(pair);
    pair->tx = bit_buffer_alloc(tx_capacity);
    pair->rx = bit_buffer_alloc(rx_capacity);
    if(!pair->tx || !pair->rx) {
        seader_hf_buffer_pair_release_buffers(pair);
        return false;
    }

    pair->tx_capacity = tx_capacity;
    pair->rx_capacity = rx_capacity;
    return true;
}

void seader_hf_buffer_pair_free(SeaderHfBufferPair* pair) {
    if(!pair) {
        return;
    }

    seader_hf_buffer_pair_release_buffers(pair);
}
