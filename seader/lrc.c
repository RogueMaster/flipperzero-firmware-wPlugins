#include "lrc.h"

uint8_t seader_calc_lrc(uint8_t* data, size_t len) {
    uint8_t lrc = 0;
    for(size_t i = 0; i < len; i++) {
        lrc ^= data[i];
    }
    return lrc;
}

bool seader_validate_lrc(uint8_t* data, size_t len) {
    uint8_t lrc = seader_calc_lrc(data, len - 1);
    return lrc == data[len - 1];
}

size_t seader_add_lrc(uint8_t* data, size_t len) {
    data[len] = seader_calc_lrc(data, len);
    return len + 1;
}
