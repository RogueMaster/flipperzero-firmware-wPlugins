#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t* ptr;
    size_t len;
} SeaderBytesView;

typedef struct {
    uint8_t tag;
    SeaderBytesView value;
} SeaderBerTlvView;

typedef struct {
    const uint8_t* buffer;
    size_t len;
    size_t offset;
} SeaderBerCursor;

void seader_ber_cursor_init(SeaderBerCursor* cursor, const uint8_t* buffer, size_t len);

bool seader_ber_next_tlv(SeaderBerCursor* cursor, SeaderBerTlvView* tlv);

bool seader_ber_parse_uint32(SeaderBytesView value, uint32_t* out);
