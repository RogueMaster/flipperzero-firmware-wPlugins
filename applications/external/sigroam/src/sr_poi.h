#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ★ Pure logic. Must not include any furi header (ADR-003). */

enum { SR_POI_ACK_MS = 2000 }; /* Product trade-off, NOT a measured value. See card D15-POI decision 6. */

typedef enum {
    SrPoiGateOk = 0,
    SrPoiGateNoLink,      /* serial not open / codec has no POI command */
    SrPoiGateNotScanning, /* not in a wardrive session -> peer would drop it silently (P2) */
    SrPoiGateNoFix,       /* no GPS fix -> tagPOI returns silently (P3) */
} SrPoiGate;

typedef enum {
    SrPoiPhaseIdle = 0,
    SrPoiPhaseWaitAck,
    SrPoiPhaseDone,
    SrPoiPhaseNoReply,
} SrPoiPhase;

typedef struct {
    uint8_t phase;
    uint32_t ack_at_send;  /* snapshot of sr_worker_cmdack_count(SrCmdAckPoi) */
    uint32_t sent_tick_ms;
} SrPoiCtx;

/* scan_ui takes SrScanUiState values; session takes SrSessionState values.
   Both are passed as uint8_t so this header does not depend on their headers. */
static inline SrPoiGate sr_poi_gate(bool link_ok, uint8_t session, uint8_t scan_ui, bool fix);

static inline void sr_poi_reset(SrPoiCtx* ctx);

/* ack_now = current cmdack count for SrCmdAckPoi. Returns the new phase. */
static inline SrPoiPhase
    sr_poi_step(SrPoiCtx* ctx, uint32_t ack_now, uint32_t tick_ms);

/* Returns a static literal or NULL, always a single line of <= 20 chars (SR_VIEW_COLS). */
static inline const char* sr_poi_status_text(uint8_t phase, uint8_t gate);

_Static_assert(sizeof("POI sent...") - 1u <= 20u, "POI sent... exceeds 20");
_Static_assert(sizeof("POI logged") - 1u <= 20u, "POI logged exceeds 20");
_Static_assert(sizeof("POI unconfirmed") - 1u <= 20u, "POI unconfirmed exceeds 20");
_Static_assert(sizeof("POI needs GPS fix") - 1u <= 20u, "POI needs GPS fix exceeds 20");
_Static_assert(sizeof("POI: no link") - 1u <= 20u, "POI: no link exceeds 20");

/*
 * Evaluation order is fixed (link, then scanning, then fix) so each failure
 * maps to one user-readable sentence. session == 1 is SrSessionRunning;
 * scan_ui == 2 is SrScanUiState Running (card D15-POI A3 / A5).
 */
static inline SrPoiGate sr_poi_gate(bool link_ok, uint8_t session, uint8_t scan_ui, bool fix) {
    if(!link_ok) {
        return SrPoiGateNoLink;
    }
    if(session != 1u || scan_ui != 2u) {
        return SrPoiGateNotScanning;
    }
    if(!fix) {
        return SrPoiGateNoFix;
    }
    return SrPoiGateOk;
}

static inline void sr_poi_reset(SrPoiCtx* ctx) {
    if(ctx == NULL) {
        return;
    }
    ctx->phase = (uint8_t)SrPoiPhaseIdle;
    ctx->ack_at_send = 0u;
    ctx->sent_tick_ms = 0u;
}

static inline SrPoiPhase sr_poi_step(SrPoiCtx* ctx, uint32_t ack_now, uint32_t tick_ms) {
    if(ctx == NULL) {
        return SrPoiPhaseIdle;
    }
    if(ctx->phase != (uint8_t)SrPoiPhaseWaitAck) {
        return (SrPoiPhase)ctx->phase;
    }
    /* Wrapping counter: compare with !=, never with > (src/sr_types.h:258-259;
     * precedent src/sr_gps_sample.h:136-138). */
    if(ack_now != ctx->ack_at_send) {
        ctx->phase = (uint8_t)SrPoiPhaseDone;
        return SrPoiPhaseDone;
    }
    /* Unsigned subtraction: furi_get_tick overflows. See sr_scan_ctl.h:39-40. */
    if((uint32_t)(tick_ms - ctx->sent_tick_ms) >= (uint32_t)SR_POI_ACK_MS) {
        ctx->phase = (uint8_t)SrPoiPhaseNoReply;
        return SrPoiPhaseNoReply;
    }
    return SrPoiPhaseWaitAck;
}

static inline const char* sr_poi_status_text(uint8_t phase, uint8_t gate) {
    if(phase == (uint8_t)SrPoiPhaseWaitAck) {
        return "POI sent...";
    }
    if(phase == (uint8_t)SrPoiPhaseDone) {
        return "POI logged";
    }
    if(phase == (uint8_t)SrPoiPhaseNoReply) {
        /* Not "no reply": the peer DID receive it and chose to stay silent.
         * tagPOI() returns silently when its realtime getFixStatus() is false
         * (WiFiScan.cpp:5776), and upstream's explanatory else branch is commented
         * out (CommandLine.cpp:1211-1213), so App can never tell "refused" from
         * "lost". Stay neutral here; the cause is documented, not guessed at in
         * 20 columns. See WORKLOG 2026-09-04. */
        return "POI unconfirmed";
    }
    if(phase != (uint8_t)SrPoiPhaseIdle) {
        return NULL;
    }
    if(gate == (uint8_t)SrPoiGateNoFix) {
        return "POI needs GPS fix";
    }
    if(gate == (uint8_t)SrPoiGateNoLink) {
        return "POI: no link";
    }
    return NULL;
}
