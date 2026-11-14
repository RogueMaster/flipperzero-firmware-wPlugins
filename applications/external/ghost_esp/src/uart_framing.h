#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    UartFrameText,
    UartFramePcap,
    UartFrameCsv,
} UartFrameType;

typedef struct {
    UartFrameType capture_type;
    UartFrameType frame_type;
    uint8_t prefix[11];
    uint8_t prefix_len;
    uint8_t candidates;
    bool swallow_newline;
    uint8_t ansi_state;
} UartFraming;

// A zero-length output marks a frame close; data has already been emitted.
typedef void (
    *UartFramingOutput)(UartFrameType type, const uint8_t* data, size_t len, void* context);

void uart_framing_reset(UartFraming* parser, UartFrameType capture_type);
void uart_framing_feed(
    UartFraming* parser,
    const uint8_t* data,
    size_t len,
    UartFramingOutput output,
    void* context);
void uart_framing_finish(UartFraming* parser, UartFramingOutput output, void* context);
