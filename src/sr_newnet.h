#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ★ New-network tick. Must not include any furi header (ADR-003).
 *
 * One tick when unique_est rises while a session is running, at most once
 * per 2000 ms. The gap uses unsigned subtraction so the furi tick wrap does
 * not open or close the window by accident. The first eval after a reset
 * only records the current estimate. While not running, last_uniq is
 * resynced and the limiter disarms, so stop/start does not replay a burst.
 */

enum { SR_NEWNET_GAP_MS = 2000 };

typedef struct {
    uint32_t last_uniq;
    uint32_t last_fire_ms;
    bool armed;
} SrNewNetCtx;

static inline bool
    sr_newnet_eval(SrNewNetCtx* ctx, uint32_t unique_est, bool running, uint32_t now_ms) {
    bool rose;

    if(ctx == NULL) {
        return false;
    }
    if(!running) {
        ctx->last_uniq = unique_est;
        ctx->armed = false;
        return false;
    }
    if(!ctx->armed) {
        ctx->last_uniq = unique_est;
        ctx->armed = true;
        /* The gap starts at a real fire. This sync leaves the next rise eligible. */
        ctx->last_fire_ms = now_ms - (uint32_t)SR_NEWNET_GAP_MS;
        return false;
    }
    rose = unique_est > ctx->last_uniq;
    ctx->last_uniq = unique_est;
    if(!rose) {
        return false;
    }
    if((uint32_t)(now_ms - ctx->last_fire_ms) < (uint32_t)SR_NEWNET_GAP_MS) {
        return false;
    }
    ctx->last_fire_ms = now_ms;
    return true;
}
