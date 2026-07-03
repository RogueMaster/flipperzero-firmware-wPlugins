#include "tx_ratio.h"

static const uint8_t dcf77_tx_ratio_y_values[] = {
    1U,
    2U,
    3U,
    4U,
    5U,
    6U,
    7U,
    8U,
    9U,
    10U,
    12U,
    15U,
    16U,
    20U,
    24U,
};

size_t dcf77_tx_ratio_y_count(void) {
    return sizeof(dcf77_tx_ratio_y_values) / sizeof(dcf77_tx_ratio_y_values[0]);
}

uint8_t dcf77_tx_ratio_y_value(size_t index) {
    if(index >= dcf77_tx_ratio_y_count()) {
        index = 0U;
    }

    return dcf77_tx_ratio_y_values[index];
}

uint8_t dcf77_tx_ratio_y_index_for_value(uint8_t value) {
    for(size_t i = 0; i < dcf77_tx_ratio_y_count(); i++) {
        if(dcf77_tx_ratio_y_values[i] == value) {
            return (uint8_t)i;
        }
    }

    return 0U;
}

uint8_t dcf77_tx_ratio_x_clamp(uint8_t x, uint8_t y) {
    if(x == 0U) {
        return 1U;
    }

    if(y == 0U) {
        return 1U;
    }

    if(x > y) {
        return y;
    }

    return x;
}

bool dcf77_tx_ratio_should_send(uint32_t frame_index, uint8_t x, uint8_t y) {
    x = dcf77_tx_ratio_x_clamp(x, y);
    y = dcf77_tx_ratio_x_clamp(y, y);

    return (((frame_index + 1U) * x + y - 1U) / y) != ((frame_index * x + y - 1U) / y);
}
