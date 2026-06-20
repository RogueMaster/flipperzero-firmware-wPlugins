#pragma once

#include <stddef.h>
#include <stdint.h>

uint32_t seader_uart_rx_inter_chunk_delay_ms(size_t received_len);
size_t seader_uart_rx_discard_consumed(uint8_t* buffer, size_t len, size_t consumed);
