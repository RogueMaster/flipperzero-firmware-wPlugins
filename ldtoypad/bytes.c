#include "bytes.h"

uint32_t readUInt32LE(const uint8_t* buffer, int offset) {
    return ((uint32_t)buffer[offset]) | ((uint32_t)buffer[offset + 1] << 8) |
           ((uint32_t)buffer[offset + 2] << 16) | ((uint32_t)buffer[offset + 3] << 24);
}

uint32_t readUInt32BE(const uint8_t* buffer, int offset) {
    return ((uint32_t)buffer[offset] << 24) | ((uint32_t)buffer[offset + 1] << 16) |
           ((uint32_t)buffer[offset + 2] << 8) | ((uint32_t)buffer[offset + 3]);
}

void writeUInt16LE(uint8_t* buffer, uint16_t value, int offset) {
    buffer[offset] = value & 0xFF;
    buffer[offset + 1] = (value >> 8) & 0xFF;
}

void writeUInt16BE(uint8_t* buffer, uint16_t value, int offset) {
    buffer[offset] = (value >> 8) & 0xFF;
    buffer[offset + 1] = value & 0xFF;
}

void writeUInt32LE(uint8_t* buffer, uint32_t value, int offset) {
    buffer[offset] = value & 0xFF;
    buffer[offset + 1] = (value >> 8) & 0xFF;
    buffer[offset + 2] = (value >> 16) & 0xFF;
    buffer[offset + 3] = (value >> 24) & 0xFF;
}

void writeUInt32BE(uint8_t* buffer, uint32_t value, int offset) {
    buffer[offset] = (value >> 24) & 0xFF;
    buffer[offset + 1] = (value >> 16) & 0xFF;
    buffer[offset + 2] = (value >> 8) & 0xFF;
    buffer[offset + 3] = value & 0xFF;
}