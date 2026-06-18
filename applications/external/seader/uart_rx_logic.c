#include "uart_rx_logic.h"

#include <string.h>

uint32_t seader_uart_rx_inter_chunk_delay_ms(size_t received_len) {
    (void)received_len;
    return 0U;
}

size_t seader_uart_rx_discard_consumed(uint8_t* buffer, size_t len, size_t consumed) {
    if(!buffer) {
        return 0U;
    }
    if(consumed == 0U) {
        return len;
    }
    if(consumed >= len) {
        return 0U;
    }

    size_t remaining = len - consumed;
    memmove(buffer, buffer + consumed, remaining);
    return remaining;
}
