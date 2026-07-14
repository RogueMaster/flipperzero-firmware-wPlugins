#include "game.hpp"

int16_t mul_f8_s16(int16_t a, uint8_t b) {
    return (int16_t)(((int32_t)a * (uint32_t)b) >> 8);
}

int16_t mul_f8_s16(int16_t a, uint16_t b) {
    return (int16_t)(((int32_t)a * (uint32_t)b) >> 8);
}

int16_t mul_f8_s16(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * (int32_t)b) >> 8);
}

int16_t mul_f16_s16(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * (int32_t)b) >> 16);
}

uint16_t mul_f8_u16(uint16_t a, uint8_t b) {
    return (uint16_t)(((uint32_t)a * (uint32_t)b) >> 8);
}

uint16_t mul_f8_u16(uint16_t a, uint16_t b) {
    return (uint16_t)(((uint32_t)a * (uint32_t)b) >> 8);
}

int16_t mul_f7_s16(int16_t a, int8_t b) {
    return (int16_t)(((int32_t)a * (int32_t)b) >> 7);
}

int16_t mul_f15_s16(int16_t a, int16_t b) {
    return (int16_t)(((int32_t)a * (int32_t)b) >> 15);
}
