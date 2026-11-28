#include "tpms_checksum.h"

uint8_t tpms_crc8(const uint8_t* message, size_t length, uint8_t polynomial, uint8_t init) {
    uint8_t remainder = init;
    for(size_t byte = 0; byte < length; ++byte) {
        remainder ^= message[byte];
        for(unsigned bit = 0; bit < 8U; ++bit) {
            remainder = (remainder & 0x80U) ?
                            (uint8_t)((uint8_t)(remainder << 1U) ^ polynomial) :
                            (uint8_t)(remainder << 1U);
        }
    }
    return remainder;
}

uint16_t tpms_crc16(
    const uint8_t* message,
    size_t length,
    uint16_t polynomial,
    uint16_t init) {
    uint16_t remainder = init;
    for(size_t byte = 0; byte < length; ++byte) {
        remainder ^= (uint16_t)message[byte] << 8U;
        for(unsigned bit = 0; bit < 8U; ++bit) {
            remainder = (remainder & 0x8000U) ?
                            (uint16_t)((uint16_t)(remainder << 1U) ^ polynomial) :
                            (uint16_t)(remainder << 1U);
        }
    }
    return remainder;
}

uint8_t tpms_xor_bytes(const uint8_t* message, size_t length) {
    uint8_t value = 0U;
    for(size_t i = 0; i < length; ++i) value ^= message[i];
    return value;
}

uint16_t tpms_add_bytes(const uint8_t* message, size_t length) {
    uint16_t value = 0U;
    for(size_t i = 0; i < length; ++i) value = (uint16_t)(value + message[i]);
    return value;
}
