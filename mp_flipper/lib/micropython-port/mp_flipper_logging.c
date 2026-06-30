#include <furi.h>

#include <mp_flipper_logging.h>
#include <mp_flipper_runtime.h>

#include "mp_flipper_context.h"

static inline FuriLogLevel decode_log_level(uint8_t level) {
    switch(level) {
    case MP_FLIPPER_LOG_LEVEL_TRACE:
        return FuriLogLevelTrace;
    case MP_FLIPPER_LOG_LEVEL_DEBUG:
        return FuriLogLevelDebug;
    case MP_FLIPPER_LOG_LEVEL_INFO:
        return FuriLogLevelInfo;
    case MP_FLIPPER_LOG_LEVEL_WARN:
        return FuriLogLevelWarn;
    case MP_FLIPPER_LOG_LEVEL_ERROR:
        return FuriLogLevelError;
    case MP_FLIPPER_LOG_LEVEL_NONE:
        return FuriLogLevelNone;
    default:
        return FuriLogLevelNone;
    }
}

inline uint8_t mp_flipper_log_get_effective_level() {
    switch(furi_log_get_level()) {
    case FuriLogLevelTrace:
        return MP_FLIPPER_LOG_LEVEL_TRACE;
    case FuriLogLevelDebug:
        return MP_FLIPPER_LOG_LEVEL_DEBUG;
    case FuriLogLevelInfo:
        return MP_FLIPPER_LOG_LEVEL_INFO;
    case FuriLogLevelWarn:
        return MP_FLIPPER_LOG_LEVEL_WARN;
    case FuriLogLevelError:
        return MP_FLIPPER_LOG_LEVEL_ERROR;
    case FuriLogLevelNone:
        return MP_FLIPPER_LOG_LEVEL_NONE;
    default:
        return MP_FLIPPER_LOG_LEVEL_NONE;
    }
}

inline void mp_flipper_log(uint8_t raw_level, const char* message) {
    FuriLogLevel level = decode_log_level(raw_level);

    furi_log_print_format(level, "uPython", message);
}
