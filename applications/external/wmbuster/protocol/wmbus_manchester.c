#include "wmbus_manchester.h"

size_t wmbus_manchester_decode(
    const uint8_t* chips,
    size_t chip_bits,
    WmbusManchesterPolarity pol,
    uint8_t* out,
    size_t out_capacity_bits,
    size_t* errors) {
    size_t pairs = chip_bits / 2;
    if(pairs > out_capacity_bits) pairs = out_capacity_bits;
    size_t err = 0;

    for(size_t i = 0; i < pairs; i++) {
        size_t bit = i * 2;
        uint8_t a = (chips[bit >> 3] >> (7 - (bit & 7))) & 1u;
        uint8_t b = (chips[(bit + 1) >> 3] >> (7 - ((bit + 1) & 7))) & 1u;

        uint8_t v;
        if(a == 0 && b == 1) {
            v = (pol == WmbusManchesterIeee802) ? 1 : 0;
        } else if(a == 1 && b == 0) {
            v = (pol == WmbusManchesterIeee802) ? 0 : 1;
        } else {
            err++;
            v = 0;
        }
        size_t obit = i;
        if(v) out[obit >> 3] |= (uint8_t)(1u << (7 - (obit & 7)));
        else  out[obit >> 3] &= (uint8_t)~(1u << (7 - (obit & 7)));
    }
    if(errors) *errors = err;
    return pairs;
}
