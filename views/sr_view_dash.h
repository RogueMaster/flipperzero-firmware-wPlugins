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
    /* Fits in the 3-byte pad before ap_wifi. From
     * sr_scan_ctl_sealing_ex: stop latch / Diag 2/3 / post-stop Busy 2/3. */
    bool board_sealing;
    /* ---- Dash tab ---- */
    uint32_t ap_wifi, ap_ble, unique_est, with_gps_fix;
    uint32_t ap_24, ap_5;
    uint32_t rx_bytes, rx_dropped, rx_max_fill;
    uint32_t elapsed_ms; /* now_tick - started_tick_ms; 0 when the session is not Running */
    uint32_t last_elapsed_ms; /* frozen at Running->Stopped; 0 after reset_session */
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
    SrQualInfo qual;   /* Last Qual: snapshot; unused when qual_rev == 0 */
    uint32_t qual_rev; /* 0 = never seen (generic Marauder: keep the count view) */
    /* F2 rev2. Mirrors of SrModel fields (dash_fill copies them under app->mtx), fed to
     * sr_fmt_qual_fresh() by the render gate in views/sr_view_dash.c. */
    uint32_t qual_tick_ms; /* Mirrors SrModel.qual_tick_ms -- furi tick of the last Qual: */
    uint32_t sess_ms;      /* Mirrors SrModel.sess.ms -- 0 = board reports no session (§1C) */
    SrRadioInfo radio;     /* Mirrors SrModel.radio -- permission bits; unused when radio_rev==0 */
    uint32_t radio_rev;    /* 0 = Radio: never seen (unknown, not BLE OFF) */
    uint32_t up_q;         /* Mirrors SrModel.up.q; meaningful when up_known */
    uint8_t cfg_key;
    uint8_t cfg_home;
    bool up_known;
    bool cfg_known;
    bool pending_prompt; /* Idle pending-survey popup; computed in dash_fill */
} SrDashModel;

_Static_assert(sizeof(SrDashModel) <= 768, "SrDashModel over 768 B (T4.1 / ADR-019)");

/* F2 rev2 §3. Column budget for the F2 headline only, decoupled from SR_VIEW_COLS=20
 * (which the 23-char OK string no longer fits). The retreat loop in
 * views/sr_view_dash.c starts here and only shrinks -- see sr_view_fmt.h's
 * sr_view_fmt_health doc comment. */
enum { SR_HEALTH_COLS_MAX = 32 };

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
void sr_view_dash_set_back_callback(SrViewDash* d, SrViewDashCallback cb, void* context);
#endif
