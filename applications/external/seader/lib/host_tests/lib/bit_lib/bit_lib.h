#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BitLibParityEven = 0,
    BitLibParityOdd = 1,
} BitLibParity;

static inline bool bit_lib_test_parity_32(uint32_t value, BitLibParity parity) {
    const bool odd = (__builtin_popcount(value) & 1U) != 0U;
    return (parity == BitLibParityOdd) ? odd : !odd;
}
