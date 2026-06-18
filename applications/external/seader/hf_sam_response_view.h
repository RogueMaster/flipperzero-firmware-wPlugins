#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t* data;
    size_t data_len;
    uint16_t protocol;
    uint32_t timeout_us;
    const uint8_t* format;
    size_t format_len;
} SeaderHfSamNfcSendView;

bool seader_hf_sam_response_view_parse_nfc_send(
    const uint8_t* response,
    size_t response_len,
    SeaderHfSamNfcSendView* out);
