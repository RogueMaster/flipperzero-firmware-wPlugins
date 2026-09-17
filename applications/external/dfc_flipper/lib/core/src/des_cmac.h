#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dfc_port.h"

// key_len must be 8 (single DES), 16 (2-key 3DES) or 24 (3-key 3DES); the underlying
// block cipher used for CMAC subkey generation and chaining is selected accordingly.
bool des_cmac(uint8_t* key, size_t key_len, uint8_t* message, size_t message_len, uint8_t* cmac);
bool des_cmac_with_iv(
    uint8_t* key,
    size_t key_len,
    uint8_t* message,
    size_t message_len,
    uint8_t* iv,
    uint8_t* cmac);
