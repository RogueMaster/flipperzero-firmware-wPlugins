#include "uhf_protocol.h"

#include <stdio.h>
#include <string.h>

static int8_t uhf_protocol_hex_digit_value(char c) {
    if(c >= '0' && c <= '9') return (int8_t)(c - '0');
    if(c >= 'A' && c <= 'F') return (int8_t)(c - 'A' + 10);
    if(c >= 'a' && c <= 'f') return (int8_t)(c - 'a' + 10);
    return -1;
}

uint32_t uhf_protocol_read_be(const uint8_t* data, size_t len) {
    if(!data || len > sizeof(uint32_t)) return 0U;
    uint32_t value = 0U;
    for(size_t i = 0U; i < len; i++)
        value = (value << 8U) | data[i];
    return value;
}

bool uhf_protocol_bytes_to_hex(const uint8_t* data, size_t len, char* out, size_t out_size) {
    if(!data || !out || len > (SIZE_MAX - 1U) / 2U || out_size < (len * 2U) + 1U) {
        return false;
    }
    for(size_t i = 0U; i < len; i++)
        snprintf(&out[i * 2U], 3U, "%02X", data[i]);
    out[len * 2U] = '\0';
    return true;
}

bool uhf_protocol_hex_to_bytes(const char* hex, uint8_t* out, size_t out_size, size_t* out_len) {
    if(!hex || !out || !out_len) return false;
    const size_t hex_len = strlen(hex);
    if((hex_len & 1U) || hex_len / 2U > out_size) return false;
    for(size_t i = 0U; i < hex_len / 2U; i++) {
        const int8_t high = uhf_protocol_hex_digit_value(hex[i * 2U]);
        const int8_t low = uhf_protocol_hex_digit_value(hex[i * 2U + 1U]);
        if(high < 0 || low < 0) return false;
        out[i] = (uint8_t)((high << 4U) | low);
    }
    *out_len = hex_len / 2U;
    return true;
}

bool uhf_protocol_build_ucm601_frame(
    uint8_t command,
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* out,
    size_t out_size,
    size_t* out_len) {
    if(payload_len > SIZE_MAX - 3U) return false;
    const size_t len_field = payload_len + 3U;
    const size_t total_size = len_field + 2U;
    if(!out || !out_len || (payload_len && !payload) || total_size > out_size ||
       len_field > UINT8_MAX) {
        return false;
    }
    out[0] = 0xA0U;
    out[1] = (uint8_t)len_field;
    out[2] = 0x00U;
    out[3] = command;
    if(payload_len && payload) memcpy(&out[4], payload, payload_len);
    uint32_t checksum = 0U;
    for(size_t i = 0U; i < total_size - 1U; i++)
        checksum = (checksum + out[i]) & 0xFFU;
    out[total_size - 1U] = (uint8_t)((~checksum + 1U) & 0xFFU);
    *out_len = total_size;
    return true;
}

const char* uhf_protocol_tag_error_text(uint8_t code) {
    switch(code) {
    case 0x10:
        return "OK";
    case 0x20:
        return "No tag";
    case 0x22:
        return "Wrong password";
    case 0x24:
        return "Memory locked";
    case 0x23:
        return "Memory overrun";
    case 0x31:
        return "Access failed";
    case 0x32:
        return "Read failed";
    case 0x33:
        return "Write failed";
    default:
        return "Tag operation failed";
    }
}
