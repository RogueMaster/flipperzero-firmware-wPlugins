#pragma once

#include <stddef.h>
#include <stdint.h>

#include <furi/core/log.h>

#ifdef FURI_DEBUG
#define SEADER_VERBOSE_LOG 1
#else
#define SEADER_VERBOSE_LOG 0
#endif

void seader_verbose_log(FuriLogLevel level, const char* tag, const char* fmt, ...)
    _ATTRIBUTE((__format__(__printf__, 3, 4)));
void seader_verbose_hex(
    FuriLogLevel level,
    const char* tag,
    const char* prefix,
    const uint8_t* data,
    size_t len);

#if SEADER_VERBOSE_LOG
#define SEADER_VERBOSE_D(tag, format, ...) \
    seader_verbose_log(FuriLogLevelDebug, tag, format, ##__VA_ARGS__)
#define SEADER_VERBOSE_I(tag, format, ...) \
    seader_verbose_log(FuriLogLevelInfo, tag, format, ##__VA_ARGS__)
#define SEADER_VERBOSE_HEX(level, tag, prefix, data, len) \
    seader_verbose_hex(level, tag, prefix, data, len)
#else
#define SEADER_VERBOSE_D(tag, format, ...) \
    do {                                   \
        (void)(tag);                       \
    } while(0)
#define SEADER_VERBOSE_I(tag, format, ...) \
    do {                                   \
        (void)(tag);                       \
    } while(0)
#define SEADER_VERBOSE_HEX(level, tag, prefix, data, len) \
    do {                                                  \
        (void)(level);                                    \
        (void)(tag);                                      \
        (void)(prefix);                                   \
        (void)(data);                                     \
        (void)(len);                                      \
    } while(0)
#endif

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
