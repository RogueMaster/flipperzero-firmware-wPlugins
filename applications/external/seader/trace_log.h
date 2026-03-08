#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef SEADER_ENABLE_TRACE_LOG

/* Define SEADER_ENABLE_TRACE_LOG in the app build to enable SD-card tracing. */

#define SEADER_TRACE_FILE_NAME APP_DATA_PATH("trace.log")

void seader_trace_reset(void);
void seader_trace(const char* tag, const char* fmt, ...);
void seader_trace_hex(const char* tag, const char* prefix, const uint8_t* data, size_t len);

#else

static inline void seader_trace_reset(void) {
}

static inline void seader_trace(const char* tag, const char* fmt, ...) {
    (void)tag;
    (void)fmt;
}

static inline void
    seader_trace_hex(const char* tag, const char* prefix, const uint8_t* data, size_t len) {
    (void)tag;
    (void)prefix;
    (void)data;
    (void)len;
}

#endif
