#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ★ Pure logic. Must not include any furi header (ADR-003). */

enum { SR_ALERT_MIN_GAP_MS = 3000 };

typedef enum {
    SrAlertNone = 0,
    SrAlertGpsFixLost, /* had fix -> no fix */
    SrAlertGpsFixAcquired, /* no fix -> has fix */
} SrAlertKind;

/* last_fix: 0 = unknown (no CSV row seen yet), 1 = no fix, 2 = has fix. */
typedef struct {
    uint8_t last_fix;
    uint32_t last_emit_ms;
    bool has_emitted;
} SrAlertCtx;

static inline void sr_alert_reset(SrAlertCtx* ctx) {
    if(ctx == NULL) {
        return;
    }
    ctx->last_fix = 0;
    ctx->last_emit_ms = 0;
    ctx->has_emitted = false;
}

static inline SrAlertKind
    sr_alert_eval(SrAlertCtx* ctx, uint32_t gps_csv_rev, bool fix_now, uint32_t tick_ms) {
    uint8_t now_st;

    if(ctx == NULL) {
        return SrAlertNone;
    }
    if(gps_csv_rev == 0u) {
        return SrAlertNone;
    }
    now_st = fix_now ? 2u : 1u;
    if(ctx->last_fix == 0u) {
        ctx->last_fix = now_st;
        return SrAlertNone;
    }
    if(ctx->last_fix == now_st) {
        return SrAlertNone;
    }
    /* Edge. Decision 5: last_fix updates even when the emit is swallowed by the
     * gap, so a swallowed transition is permanently lost instead of bursting
     * once the window opens. */
    ctx->last_fix = now_st;
    /* Unsigned subtraction: furi_get_tick overflows. Writing
     * tick > last_emit + GAP would mis-fire at the wraparound point. */
    if(ctx->has_emitted &&
       (uint32_t)(tick_ms - ctx->last_emit_ms) < (uint32_t)SR_ALERT_MIN_GAP_MS) {
        return SrAlertNone;
    }
    ctx->last_emit_ms = tick_ms;
    ctx->has_emitted = true;
    return (now_st == 1u) ? SrAlertGpsFixLost : SrAlertGpsFixAcquired;
}
