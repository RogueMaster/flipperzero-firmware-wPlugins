/* Splits a byte stream into '\n'-terminated lines (NDJSON framing).
 *
 * Bytes accumulate in a caller-owned buffer of `cap` bytes, which holds a
 * line of up to cap - 1 bytes plus its NUL. Every complete line is handed
 * to the callback NUL-terminated, without its '\n' and a trailing '\r'.
 * Empty lines are skipped. A line that does not fit, or that contains a NUL
 * byte, is dropped up to its '\n' and counted; the next line is unaffected.
 * The callback runs inside line_buffer_feed() and must not feed the same
 * buffer. Not thread-safe.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*LineBufferCallback)(const char* line, size_t len, void* ctx);

typedef struct {
    char* buf;
    size_t cap;
    size_t len;
    bool discarding; /* inside a line being dropped */
    uint32_t dropped;
    LineBufferCallback callback;
    void* ctx;
} LineBuffer;

void line_buffer_init(
    LineBuffer* lb,
    char* storage,
    size_t cap,
    LineBufferCallback callback,
    void* ctx);

/* Consumes `len` bytes and returns the number of lines delivered. */
size_t line_buffer_feed(LineBuffer* lb, const uint8_t* data, size_t len);

/* Forgets a partial line (e.g. when the port is reopened). */
void line_buffer_reset(LineBuffer* lb);

/* Lines dropped so far for being too long or holding a NUL byte. */
uint32_t line_buffer_dropped(const LineBuffer* lb);
