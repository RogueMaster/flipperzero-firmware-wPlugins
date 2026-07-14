#include "uart_tx_logic.h"

#include <string.h>

bool seader_uart_tx_frame_copy(
    SeaderUartTxFrame* out,
    const uint8_t* data,
    size_t len,
    size_t max_len) {
    if(!out || !data || len == 0U || len > max_len || len > sizeof(out->data)) {
        return false;
    }

    out->len = len;
    memcpy(out->data, data, len);
    return true;
}
