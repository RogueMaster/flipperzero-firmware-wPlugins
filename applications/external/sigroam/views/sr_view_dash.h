#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../src/sr_types.h"
#include "../src/sr_wait_stage.h"
#include "../src/sr_view_fmt.h"
#include "../src/sr_model.h" /* SrApBrief / SR_AP_FLAG_* -- sr_model.h is ★ and compiles on the host */

/*
 * The value-snapshot POD shared by the four Dashboard tabs (ADR-019 decision 3).
 * No pointers, no FuriString, no SrModel*, no SrRawView.
 */
typedef struct {
    uint8_t tab; /* SR_VIEW_TAB_* */
    /* ---- Common state ---- */
    bool serial_open;
    uint8_t io_status; /* An SrIoStatus value, converted to wording at draw time */
    uint8_t session; /* SrSessionState */
    uint8_t scan_ui; /* SrScanUiState */
    /* ---- Dash tab ---- */
    uint32_t ap_wifi, ap_ble, unique_est, with_gps_fix;
    uint32_t rx_bytes, rx_dropped, rx_max_fill;
    uint32_t elapsed_ms; /* now_tick - started_tick_ms; 0 when the session is not Running */
    uint32_t heap_free, heap_min, heap_max_blk; /* bytes; divided by 1024u into whole KB at draw time */
    /* The Debug rows setting (T4.11 / ADR-024). true = the Dash tab appends the d=/f= and heap
     * rows and **draws no big font** (big font + 5 small rows does not fit the 53 px content area). */
    bool debug_rows;
    /* T4.10: which layer an in-flight command is stuck at. An SrWaitStage value, converted to
     * wording at draw time. */
    uint8_t wait_stage;
    /* Meaningful only when wait_stage != SrWaitStageNone: true = awaiting start, false = awaiting stop. */
    bool cmd_is_start;
    /* ---- GPS tab (used by T4.3; this card only copies, does not draw) ---- */
    SrGpsSnapshot gps;
    uint32_t gps_blocks;
    uint8_t gps_phase; /* SrGpsPhase */
    uint8_t gps_gate;  /* SrGpsGate */
    uint8_t gps_src;   /* 0 = none, 1 = gpsdata block, 2 = CSV row (D12) */
    /* ---- Session tab (used by T4.4; this card only copies, does not draw) ---- */
    uint32_t unknown_lines, malformed_lines, illegal_trans, session_rev;
    SrFirmwareInfo firmware;
    uint32_t firmware_rev;
    /* ---- Stream tab (T4.2) ---- */
    uint16_t stream_top; /* How far back from newest (in idx space); owned by the GUI, see D0 decisions 2/3 */
    uint16_t stream_count; /* The full sr_model_recent_count(), for the scrollbar and clamping */
    uint8_t stream_n; /* Valid entries in stream_rows[], <= SR_STREAM_ROWS */
    SrApBrief stream_rows[SR_STREAM_ROWS]; /* rows[0] = idx stream_top (newer), proceeding toward older */
} SrDashModel;

_Static_assert(sizeof(SrDashModel) <= 768, "SrDashModel over 768 B (T4.1 / ADR-019)");

#ifndef SR_HOST_TEST
#include <gui/view.h>

typedef struct SrViewDash SrViewDash;

typedef void (*SrViewDashCallback)(void* context);

SrViewDash* sr_view_dash_alloc(void);
void sr_view_dash_free(SrViewDash* d);
View* sr_view_dash_get_view(SrViewDash* d);
void sr_view_dash_set(View* v, const SrDashModel* src);
void sr_view_dash_set_callback(SrViewDash* d, SrViewDashCallback cb, void* context);
void sr_view_dash_set_ok_callback(SrViewDash* d, SrViewDashCallback cb, void* context);
#endif
