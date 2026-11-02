#include "fn_wire.h"

uint16_t fn_wire_u16le_get(const uint8_t* p) {
    if(!p) return 0;
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void fn_wire_u16le_put(uint8_t* p, uint16_t v) {
    if(!p) return;
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

uint32_t fn_wire_u32le_get(const uint8_t* p) {
    if(!p) return 0;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

void fn_wire_u32le_put(uint8_t* p, uint32_t v) {
    if(!p) return;
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

uint16_t fn_wire_crc16_ccitt(const uint8_t* data, size_t len) {
    if(!data || len == 0) return 0;

    uint16_t crc = 0xFFFF;
    for(size_t i = 0; i < len; ++i) {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for(uint8_t bit = 0; bit < 8; ++bit) {
            if(crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}
