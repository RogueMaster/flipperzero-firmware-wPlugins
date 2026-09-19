#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool uhf_protocol_build_ucm601_frame(
    uint8_t command,
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);
uint32_t uhf_protocol_read_be(const uint8_t* data, size_t len);
bool uhf_protocol_bytes_to_hex(const uint8_t* data, size_t len, char* out, size_t out_size);
bool uhf_protocol_hex_to_bytes(const char* hex, uint8_t* out, size_t out_size, size_t* out_len);
const char* uhf_protocol_tag_error_text(uint8_t code);
