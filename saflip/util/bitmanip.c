#include "bitmanip.h"

void insert_bits(uint8_t* data, size_t start_bit, size_t num_bits, uint32_t value) {
    for(size_t i = 0; i < num_bits; i++) {
        size_t current_bit = start_bit + i;
        size_t byte_index = current_bit / 8;
        size_t bit_index = 7 - (current_bit % 8);

        uint32_t bit_value = (value >> (num_bits - 1 - i)) & 1U;

        data[byte_index] = (data[byte_index] & ~(1 << bit_index)) | (bit_value << bit_index);
    }
}

uint32_t extract_bits(const uint8_t* data, size_t start_bit, size_t num_bits) {
    uint32_t result = 0;
    for(size_t i = 0; i < num_bits; i++) {
        size_t byte_index = (start_bit + i) / 8;
        size_t bit_index = (start_bit + i) % 8;
        if(data[byte_index] & (1 << (7 - bit_index))) {
            result |= (1ULL << (num_bits - 1 - i));
        }
    }
    return result;
}
