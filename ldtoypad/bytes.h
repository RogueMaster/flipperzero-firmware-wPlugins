#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t readUInt32LE(const uint8_t* buffer, int offset);
uint32_t readUInt32BE(const uint8_t* buffer, int offset);

void writeUInt16LE(uint8_t* buffer, uint16_t value, int offset);
void writeUInt16BE(uint8_t* buffer, uint16_t value, int offset);

void writeUInt32LE(uint8_t* buffer, uint32_t value, int offset);
void writeUInt32BE(uint8_t* buffer, uint32_t value, int offset);

#ifdef __cplusplus
}
#endif