#include "line_buffer.h"

void line_buffer_init(
    LineBuffer* lb,
    char* storage,
    size_t cap,
    LineBufferCallback callback,
    void* ctx) {
    lb->buf = storage;
    lb->cap = cap;
    lb->dropped = 0;
    lb->callback = callback;
    lb->ctx = ctx;
    line_buffer_reset(lb);
}

void line_buffer_reset(LineBuffer* lb) {
    lb->len = 0;
    lb->discarding = false;
    if(lb->cap > 0) lb->buf[0] = '\0';
}

uint32_t line_buffer_dropped(const LineBuffer* lb) {
    return lb->dropped;
}

static void drop_line(LineBuffer* lb) {
    lb->len = 0;
    lb->discarding = true;
    lb->dropped++;
}

/* Delivers the accumulated line, if any, and starts the next one. */
static size_t end_line(LineBuffer* lb) {
    size_t len = lb->len;
    bool discarding = lb->discarding;
    lb->len = 0;
    lb->discarding = false;
    if(discarding) return 0;
    if(len > 0 && lb->buf[len - 1] == '\r') len--;
    if(len == 0) return 0;
    lb->buf[len] = '\0';
    lb->callback(lb->buf, len, lb->ctx);
    return 1;
}

size_t line_buffer_feed(LineBuffer* lb, const uint8_t* data, size_t len) {
    size_t lines = 0;
    for(size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        if(byte == '\n') {
            lines += end_line(lb);
        } else if(lb->discarding) {
            continue;
        } else if(byte == '\0' || lb->len + 1 >= lb->cap) {
            drop_line(lb);
        } else {
            lb->buf[lb->len++] = (char)byte;
        }
    }
    return lines;
}
