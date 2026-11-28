#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t tpms_crc8(const uint8_t* message, size_t length, uint8_t polynomial, uint8_t init);
uint16_t tpms_crc16(const uint8_t* message, size_t length, uint16_t polynomial, uint16_t init);
uint8_t tpms_xor_bytes(const uint8_t* message, size_t length);
uint16_t tpms_add_bytes(const uint8_t* message, size_t length);

#ifdef __cplusplus
}
#endif
