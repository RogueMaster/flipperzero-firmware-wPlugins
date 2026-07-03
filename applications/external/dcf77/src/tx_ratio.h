#ifndef DCF77_TX_RATIO_H
#define DCF77_TX_RATIO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t dcf77_tx_ratio_y_count(void);
uint8_t dcf77_tx_ratio_y_value(size_t index);
uint8_t dcf77_tx_ratio_y_index_for_value(uint8_t value);
uint8_t dcf77_tx_ratio_x_clamp(uint8_t x, uint8_t y);
bool dcf77_tx_ratio_should_send(uint32_t frame_index, uint8_t x, uint8_t y);

#endif
