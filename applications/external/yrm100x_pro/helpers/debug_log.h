#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Production build: diagnostics are removed at preprocessing time.
 *
 * The false branches keep diagnostic-only local variables syntactically used,
 * while the compiler emits no calls, formatting strings, buffers, files, or
 * mutexes. This preserves the tested RFID control flow without its RAM cost.
 */
typedef enum {
    UHFDebugTrace,
    UHFDebugInfo,
    UHFDebugWarn,
    UHFDebugError,
} UHFDebugLevel;

static inline void uhf_debug_discard(const char* component, ...) {
    (void)component;
}

static inline void uhf_debug_hex_discard(
    UHFDebugLevel level,
    const char* component,
    const char* label,
    const uint8_t* data,
    size_t length) {
    (void)level;
    (void)component;
    (void)label;
    (void)data;
    (void)length;
}

static inline size_t uhf_debug_heap_discard(
    UHFDebugLevel level,
    const char* scope,
    const char* phase,
    size_t scope_base) {
    (void)level;
    (void)scope;
    (void)phase;
    (void)scope_base;
    return 0U;
}

#define UHF_T(component, ...)                                \
    do {                                                     \
        if(false) uhf_debug_discard(component, __VA_ARGS__); \
    } while(0)
#define UHF_I(component, ...) UHF_T(component, __VA_ARGS__)
#define UHF_W(component, ...) UHF_T(component, __VA_ARGS__)
#define UHF_E(component, ...) UHF_T(component, __VA_ARGS__)

#define uhf_debug_hex(...)                            \
    do {                                              \
        if(false) uhf_debug_hex_discard(__VA_ARGS__); \
    } while(0)
#define uhf_debug_heap(...) uhf_debug_heap_discard(__VA_ARGS__)
#define uhf_debug_flush()   ((void)0)

/* Strip Flipper console logging from this application as well. */
#ifdef FURI_LOG_D
#undef FURI_LOG_D
#endif
#ifdef FURI_LOG_I
#undef FURI_LOG_I
#endif
#ifdef FURI_LOG_W
#undef FURI_LOG_W
#endif
#ifdef FURI_LOG_E
#undef FURI_LOG_E
#endif

#define FURI_LOG_D(...) UHF_T(__VA_ARGS__)
#define FURI_LOG_I(...) UHF_T(__VA_ARGS__)
#define FURI_LOG_W(...) UHF_T(__VA_ARGS__)
#define FURI_LOG_E(...) UHF_T(__VA_ARGS__)
