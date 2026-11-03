/**
 * suite_helpers.h -- Shared inline helpers for the flipper_suite apps.
 *
 * Include from any app; no .c companion needed.
 */
#pragma once

#include <gui/view_dispatcher.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Remove views [0 .. count) from `vd`.
 * Works with any app whose view-id enum is dense and 0-based,
 * ending with a trailing ...ViewCount member.
 */
static inline void view_dispatcher_remove_all(ViewDispatcher* vd, uint32_t count) {
    for(uint32_t i = 0; i < count; i++) {
        view_dispatcher_remove_view(vd, i);
    }
}

/**
 * Format a byte array as a space-separated hex string.
 * `dst` must be at least len * 3 + 1 bytes.
 * Produces "0A 1B 2C " (trailing space on each byte).
 */
static inline void suite_bytes_to_hex(const uint8_t* src, size_t len, char* dst, size_t dst_size) {
    size_t pos = 0;
    for(size_t i = 0; i < len; i++) {
        if(pos + 3 >= dst_size) break;
        int n = snprintf(dst + pos, dst_size - pos, "%02X ", src[i]);
        if(n > 0) pos += (size_t)n;
    }
    if(pos > 0 && pos < dst_size)
        dst[pos] = '\0';
    else if(dst_size > 0)
        dst[0] = '\0';
}

#ifdef __cplusplus
}
#endif
