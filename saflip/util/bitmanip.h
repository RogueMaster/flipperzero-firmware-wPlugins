#include <stdlib.h>
#include <stdint.h>

void insert_bits(uint8_t* data, size_t start_bit, size_t num_bits, uint32_t value);
uint32_t extract_bits(const uint8_t* data, size_t start_bit, size_t num_bits);
