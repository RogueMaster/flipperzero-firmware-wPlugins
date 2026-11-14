#include "uart_framing.h"
#include <string.h>

static const char markers[4][12] = {"[BUF/BEGIN]", "[BUF/CLOSE]", "[CSV/BEGIN]", "[CSV/CLOSE]"};

// Strip ANSI only from console text. Keep state across RX chunks; capture
// payloads bypass this path completely, including any literal ESC bytes.
static void uart_emit(
    UartFraming* p,
    UartFrameType type,
    const uint8_t* data,
    size_t len,
    UartFramingOutput output,
    void* context) {
    if(type != UartFrameText || !len) {
        output(type, data, len, context);
        return;
    }
    for(size_t i = 0; i < len; ++i) {
        uint8_t c = data[i];
        if(!p->ansi_state) {
            if(c == 0x1b) {
                p->ansi_state = 1;
                continue;
            }
            size_t end = i + 1;
            while(end < len && data[end] != 0x1b)
                ++end;
            output(type, data + i, end - i, context);
            i = end - 1;
        } else if(c == '\n' || c == '\r') {
            p->ansi_state = 0;
            output(type, data + i, 1, context);
        } else if(p->ansi_state == 1) {
            p->ansi_state = c == '['                 ? 2 :
                            c == ']'                 ? 3 :
                            (c >= 0x20 && c <= 0x2f) ? 4 :
                            c == 0x1b                ? 1 :
                                                       0;
        } else if(p->ansi_state == 2) {
            if(c == 0x1b)
                p->ansi_state = 1;
            else if(c >= 0x40 && c <= 0x7e)
                p->ansi_state = 0;
        } else if(p->ansi_state == 3) {
            if(c == 7)
                p->ansi_state = 0;
            else if(c == 0x1b)
                p->ansi_state = 5;
        } else if(p->ansi_state == 4) {
            if(c >= 0x30 && c <= 0x7e) p->ansi_state = 0;
        } else if(p->ansi_state == 5) {
            p->ansi_state = c == '\\' ? 0 : c == 0x1b ? 5 : 3;
        }
    }
}

void uart_framing_reset(UartFraming* p, UartFrameType capture_type) {
    memset(p, 0, sizeof(*p));
    p->capture_type = capture_type;
}

void uart_framing_feed(
    UartFraming* p,
    const uint8_t* data,
    size_t len,
    UartFramingOutput output,
    void* context) {
    for(size_t i = 0; i < len; ++i) {
        uint8_t c = data[i];
        if(p->swallow_newline) {
            p->swallow_newline = false;
            if(c == '\n') continue;
        }
        if(p->prefix_len) {
            uint8_t candidates = 0;
            for(size_t m = 0; m < 4; ++m) {
                if((p->candidates & (1U << m)) && c == (uint8_t)markers[m][p->prefix_len]) {
                    candidates |= 1U << m;
                }
            }
            if(candidates) {
                p->prefix[p->prefix_len++] = c;
                p->candidates = candidates;
                if(p->prefix_len == sizeof(p->prefix)) {
                    if(candidates & 1U) {
                        p->ansi_state = 0;
                        p->frame_type = p->capture_type == UartFrameCsv ? UartFrameCsv :
                                                                          UartFramePcap;
                    } else if(candidates & 4U) {
                        p->ansi_state = 0;
                        p->frame_type = UartFrameCsv;
                    } else {
                        output(p->frame_type, NULL, 0, context);
                        p->frame_type = UartFrameText;
                        p->ansi_state = 0;
                        p->swallow_newline = true;
                    }
                    p->prefix_len = 0;
                    p->candidates = 0;
                }
                continue;
            }
            // Preserve literal brackets and partial marker prefixes in payloads.
            uart_emit(p, p->frame_type, p->prefix, p->prefix_len, output, context);
            p->prefix_len = 0;
            p->candidates = 0;
        }
        if(c == '[') {
            p->prefix[0] = c;
            p->prefix_len = 1;
            p->candidates = 15;
        } else {
            // Emit runs of ordinary bytes together. Framing can span any number
            // of input chunks without retaining a second copy of their data.
            size_t end = i + 1;
            while(end < len && data[end] != '[')
                ++end;
            uart_emit(p, p->frame_type, data + i, end - i, output, context);
            i = end - 1;
        }
    }
}

void uart_framing_finish(UartFraming* p, UartFramingOutput output, void* context) {
    if(p->prefix_len) uart_emit(p, p->frame_type, p->prefix, p->prefix_len, output, context);
    uart_framing_reset(p, UartFrameText);
}
