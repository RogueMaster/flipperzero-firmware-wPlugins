#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    DfcAn10922Aes128,
    DfcAn10922Tdea2,
    DfcAn10922Tdea3,
} DfcAn10922Algorithm;

// Returns the master and diversified-key length for an algorithm, or zero for
// an invalid value.
size_t dfc_an10922_key_length(DfcAn10922Algorithm algorithm);

// Diversifies one DESFire key. AES-128 accepts 1..31 input bytes; two-key and
// three-key TDEA accept 1..15. TDEA outputs preserve the DESFire key-version
// bits carried in the low bit of the first eight master-key bytes.
//
// The input, master key and output may overlap. On a cryptographic or input
// failure, a sufficiently large output is cleared. No heap allocation occurs.
bool dfc_an10922_diversify(
    DfcAn10922Algorithm algorithm,
    const uint8_t* master_key,
    size_t master_key_len,
    const uint8_t* diversification_input,
    size_t diversification_input_len,
    uint8_t* diversified_key,
    size_t diversified_key_capacity);
