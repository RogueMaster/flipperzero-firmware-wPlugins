#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t tag;
    bool constructed;
    const uint8_t* value;
    size_t length;
    int depth;
} BerTlvField;

typedef bool (*BerTlvCallback)(const BerTlvField* field, void* ctx);

bool ber_tlv_walk(const uint8_t* buf, size_t len, BerTlvCallback cb, void* ctx);

bool ber_tlv_find(
    const uint8_t* buf,
    size_t len,
    uint32_t tag,
    const uint8_t** out_value,
    size_t* out_len);

size_t ber_tlv_read_tag(const uint8_t* buf, size_t len, uint32_t* out_tag);
size_t ber_tlv_read_length(const uint8_t* buf, size_t len, size_t* out_length);

#ifdef __cplusplus
}
#endif
