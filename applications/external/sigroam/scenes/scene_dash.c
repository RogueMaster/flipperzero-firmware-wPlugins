#include "../sigroam.h"

#include <string.h>

enum {
    SigRoamDashEventScroll = 0,
    SigRoamDashEventOk = 1,
};

static void dash_fill(SigRoamApp* app, SrDashModel* snap) {
    SrIoStats st;
    SrScanCtlCtx ctx;
    uint32_t now;
    uint8_t tab = (uint8_t)SR_VIEW_TAB_DASH;
    uint16_t top = 0;
    uint16_t count;
    uint8_t n = 0;
    uint8_t i;
    size_t rc;
    View* v;
    const SrSourceCodec* codec;
    bool link_ok;

    memset(snap, 0, sizeof(*snap));

    v = sr_view_dash_get_view(app->dash);
    if(v != NULL) {
        with_view_model(
            v,
            SrDashModel* cur,
            {
                if(cur != NULL) {
                    tab = cur->tab;
                    top = cur->stream_top;
                }
            },
            false);
    }
    snap->tab = tab;

    snap->serial_open = (app->io != NULL && sr_io_is_open(app->io));
    snap->io_status = (uint8_t)app->io_status;
    snap->session = (uint8_t)app->model.session;

    /* Read-only on app->scan: copy to the stack before eval, leaving the Drive page's exclusive fields untouched. */
    ctx = app->scan;
    ctx.session_rev_now = app->model.session_rev;
    ctx.session_now = app->model.session;
    now = furi_get_tick();
    snap->scan_ui = (uint8_t)sr_scan_ctl_eval(&ctx, now);

    snap->ap_wifi = app->model.ap_wifi;
    snap->ap_ble = app->model.ap_ble;
    snap->unique_est = app->model.unique_est;
    snap->with_gps_fix = app->model.with_gps_fix;

    memset(&st, 0, sizeof(st));
    if(app->io != NULL) {
        sr_io_get_stats(app->io, &st);
    }
    snap->rx_bytes = st.rx_bytes;
    snap->rx_dropped = st.rx_dropped;
    snap->rx_max_fill = st.rx_max_fill;

    {
        SrWaitCtx wc;
        wc.cmd_pending = app->scan.cmd_pending;
        wc.rx_bytes = st.rx_bytes; /* NOTE: must be taken after sr_io_get_stats */
        wc.cmdack_now = sr_worker_cmdack_count(
            app->worker, app->scan.cmd_is_start ? SrCmdAckStart : SrCmdAckStop);
        wc.cmdack_at_send = app->scan_cmdack_at_send;
        snap->wait_stage = (uint8_t)sr_wait_stage_eval(&wc);
        snap->cmd_is_start = app->scan.cmd_is_start;
    }

    if(app->model.session == SrSessionRunning) {
        /* Unsigned subtraction: the furi tick wraps. See sr_scan_ctl.h:39-40. */
        snap->elapsed_ms = now - app->model.started_tick_ms;
    } else {
        snap->elapsed_ms = 0;
    }

    /* app->settings is GUI-thread exclusive (sigroam.h:90-96); we are on the GUI thread here, so read it directly. */
    snap->debug_rows = app->settings.debug_rows;

    snap->heap_free = (uint32_t)memmgr_get_free_heap();
    snap->heap_min = (uint32_t)memmgr_get_minimum_free_heap();
    snap->heap_max_blk = (uint32_t)memmgr_heap_get_max_free_block();

    snap->gps_blocks = app->model.gps_blocks;
    if(app->model.session == SrSessionRunning && app->model.gps_csv_rev > 0) {
        snap->gps.fix = app->model.gps_csv.fix;
        sr_strlcpy(snap->gps.lat, sizeof(snap->gps.lat), app->model.gps_csv.lat);
        sr_strlcpy(snap->gps.lon, sizeof(snap->gps.lon), app->model.gps_csv.lon);
        sr_strlcpy(snap->gps.alt, sizeof(snap->gps.alt), app->model.gps_csv.alt);
        sr_strlcpy(snap->gps.acc, sizeof(snap->gps.acc), app->model.gps_csv.acc);
        sr_strlcpy(snap->gps.datetime, sizeof(snap->gps.datetime), app->model.gps_csv.datetime);
        snap->gps.sats[0] = '\0';
        snap->gps.text[0] = '\0';
        snap->gps_src = 2;
    } else if(app->model.gps_blocks > 0) {
        snap->gps = app->model.gps;
        snap->gps_src = 1;
    } else {
        snap->gps_src = 0;
    }
    codec = sigroam_codec(app);
    link_ok = snap->serial_open && codec != NULL && codec->build_gps_cmd != NULL;
    snap->gps_phase = app->gps_sample.phase;
    snap->gps_gate = (uint8_t)sr_gps_gate(
        link_ok, (uint8_t)app->model.session, snap->scan_ui, app->gps_sample.phase);
    /* During an active wardrive the GPS-sample fields are unused (sampling is
     * gated off). Reuse them for POI phase/gate so the existing GPS-tab hint
     * line can show POI copy without growing SrDashModel (pinned at 644). */
    if(app->model.session == SrSessionRunning &&
       snap->scan_ui == (uint8_t)SrScanUiRunning) {
        bool poi_link = snap->serial_open && codec != NULL && codec->build_poi_cmd != NULL;
        snap->gps_phase = app->poi.phase;
        snap->gps_gate = (uint8_t)sr_poi_gate(
            poi_link,
            (uint8_t)app->model.session,
            snap->scan_ui,
            snap->gps.fix);
    }
    snap->unknown_lines = app->model.unknown_lines;
    snap->malformed_lines = app->model.malformed_lines;
    snap->illegal_trans = app->model.illegal_trans;
    snap->session_rev = app->model.session_rev;
    snap->firmware = app->model.firmware;
    snap->firmware_rev = app->model.firmware_rev;

    rc = sr_model_recent_count(&app->model);
    if(rc > 0xFFFFu) {
        count = 0xFFFFu;
    } else {
        count = (uint16_t)rc;
    }
    top = sr_stream_clamp_top(top, count, (uint8_t)SR_STREAM_ROWS);
    for(i = 0; i < (uint8_t)SR_STREAM_ROWS; i++) {
        const SrApBrief* b = sr_model_recent(&app->model, (size_t)top + (size_t)i);
        if(b == NULL) {
            break;
        }
        snap->stream_rows[i] = *b;
        n++;
    }
    snap->stream_top = top;
    snap->stream_count = count;
    snap->stream_n = n;
}

void sigroam_dash_refresh(SigRoamApp* app) {
    SrDashModel snap;

    if(app == NULL || app->dash == NULL) {
        return;
    }
    dash_fill(app, &snap);
    sr_view_dash_set(sr_view_dash_get_view(app->dash), &snap);
}

static void dash_view_scroll_cb(void* context) {
    SigRoamApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SigRoamDashEventScroll);
}

static void dash_view_ok_cb(void* context) {
    SigRoamApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SigRoamDashEventOk);
}

/* The Back path does not hold app->mtx, so it must not read any field of app->model. */
static void dash_gps_abort(SigRoamApp* app) {
    const SrSourceCodec* codec;
    char cmd[SR_WORKER_CMD_MAX];
    size_t n;
    uint8_t phase;

    if(app == NULL) {
        return;
    }
    phase = app->gps_sample.phase;
    if(phase != (uint8_t)SrGpsPhaseWaitBlock && phase != (uint8_t)SrGpsPhaseWaitSlow &&
       phase != (uint8_t)SrGpsPhaseWaitStop) {
        return;
    }
    codec = sigroam_codec(app);
    if(codec != NULL && codec->build_stop_cmd != NULL) {
        n = codec->build_stop_cmd(cmd, sizeof(cmd));
        if(n == 0 || !sr_worker_send_cmd(app->worker, cmd)) {
            FURI_LOG_W(SR_TAG, "gps abort: stop cmd not queued");
        }
    } else {
        FURI_LOG_W(SR_TAG, "gps abort: no stop codec");
    }
    memset(&app->gps_sample, 0, sizeof(app->gps_sample));
}

static void dash_gps_start(SigRoamApp* app) {
    const SrSourceCodec* codec;
    char cmd[SR_WORKER_CMD_MAX];
    size_t n;
    SrScanCtlCtx ctx;
    uint8_t scan_ui;
    bool link_ok;
    bool serial_open;
    SrGpsGate g;

    serial_open = (app->io != NULL && sr_io_is_open(app->io));
    codec = sigroam_codec(app);
    link_ok = serial_open && codec != NULL && codec->build_gps_cmd != NULL;

    ctx = app->scan;
    ctx.session_rev_now = app->model.session_rev;
    ctx.session_now = app->model.session;
    scan_ui = (uint8_t)sr_scan_ctl_eval(&ctx, furi_get_tick());

    g = sr_gps_gate(link_ok, (uint8_t)app->model.session, scan_ui, app->gps_sample.phase);
    if(g != SrGpsGateOk) {
        return;
    }

    memset(&app->gps_sample, 0, sizeof(app->gps_sample));
    app->gps_sample.blocks_at_send = app->model.gps_blocks;
    app->gps_sample.sent_tick_ms = furi_get_tick();

    n = codec->build_gps_cmd(cmd, sizeof(cmd));
    if(n > 0 && sr_worker_send_cmd(app->worker, cmd)) {
        app->gps_sample.phase = (uint8_t)SrGpsPhaseWaitBlock;
    } else {
        app->gps_sample.phase = (uint8_t)SrGpsPhaseRejected;
    }
}

/* Card D15-POI B3. Snapshot the wrapping cmdack counter BEFORE send ( != , never > ). */
static void dash_poi_send(SigRoamApp* app) {
    const SrSourceCodec* codec;
    char cmd[SR_WORKER_CMD_MAX];
    size_t n;

    codec = sigroam_codec(app);
    if(codec == NULL || codec->build_poi_cmd == NULL) {
        return;
    }
    app->poi.ack_at_send = sr_worker_cmdack_count(app->worker, SrCmdAckPoi);
    app->poi.sent_tick_ms = furi_get_tick();
    n = codec->build_poi_cmd(cmd, sizeof(cmd));
    if(n > 0 && sr_worker_send_cmd(app->worker, cmd)) {
        app->poi.phase = (uint8_t)SrPoiPhaseWaitAck;
    }
}

static bool dash_poi_tick(SigRoamApp* app) {
    uint8_t before;
    uint32_t ack;

    before = app->poi.phase;
    ack = sr_worker_cmdack_count(app->worker, SrCmdAckPoi);
    (void)sr_poi_step(&app->poi, ack, furi_get_tick());
    return app->poi.phase != before;
}

/* Returns true when the phase changed, so the caller refreshes the snapshot again. */
static bool dash_gps_tick(SigRoamApp* app) {
    SrGpsStep step;
    uint8_t before;
    const SrSourceCodec* codec;
    char cmd[SR_WORKER_CMD_MAX];
    size_t n;

    /* Appended to the end of the allow list; all four phases are required */
    if(app->gps_sample.phase != (uint8_t)SrGpsPhaseWaitBlock &&
       app->gps_sample.phase != (uint8_t)SrGpsPhaseWaitSlow &&
       app->gps_sample.phase != (uint8_t)SrGpsPhaseWaitStop &&
       app->gps_sample.phase != (uint8_t)SrGpsPhaseNoReply) {
        return false;
    }

    before = app->gps_sample.phase;
    step = sr_gps_step(
        &app->gps_sample, app->model.gps_blocks, app->model.gps_stop_rev, furi_get_tick());

    if(step.act == (uint8_t)SrGpsActSendStop) {
        /* Record the confirmation snapshot first, then queue the close-out command. */
        app->gps_sample.stop_rev_at_send = app->model.gps_stop_rev;
        app->gps_sample.stop_tick_ms = furi_get_tick();
        codec = sigroam_codec(app);
        n = 0;
        if(codec != NULL && codec->build_stop_cmd != NULL) {
            n = codec->build_stop_cmd(cmd, sizeof(cmd));
        }
        if(n == 0 || !sr_worker_send_cmd(app->worker, cmd)) {
            app->gps_sample.phase = (uint8_t)SrGpsPhaseStopUnsure;
            app->gps_sample.got_block = step.got_block;
            return true;
        }
        app->gps_sample.phase = step.phase;
        app->gps_sample.got_block = step.got_block;
        return app->gps_sample.phase != before;
    }

    if(step.phase == (uint8_t)SrGpsPhaseIdle) {
        /* Invariant: phase == Idle <=> ctx all zeros (D0-4). Assigning phase alone would leave a stale blocks_at_send. */
        memset(&app->gps_sample, 0, sizeof(app->gps_sample));
        return before != (uint8_t)SrGpsPhaseIdle;
    }

    app->gps_sample.phase = step.phase;
    app->gps_sample.got_block = step.got_block;
    return app->gps_sample.phase != before;
}

/* Moved from scene_drive.c:201-231 (drive_queue_cmd). Three changes:
 *   (1) drive_serial_ready() inlined here (that file was deleted);
 *   (2) added the L2 baseline snapshot app->scan_cmdack_at_send;
 *   (3) renamed drive_ to dash_. Everything else is verbatim. */
static void dash_queue_cmd(SigRoamApp* app, bool is_start) {
    const SrSourceCodec* codec;
    char cmd[SR_WORKER_CMD_MAX];
    size_t n;

    if(app->io == NULL || !sr_io_is_open(app->io) || app->worker == NULL) {
        return;
    }
    codec = sigroam_codec(app);
    if(codec == NULL) {
        return;
    }

    if(is_start) {
        SrScanCfg cfg = {.mirror_to_serial = false};
        n = codec->build_start_cmd(&cfg, cmd, sizeof(cmd));
    } else {
        n = codec->build_stop_cmd(cmd, sizeof(cmd));
    }

    app->scan.session_rev_at_send = app->model.session_rev;
    app->scan.cmd_tick_ms = furi_get_tick();
    app->scan.cmd_is_start = is_start;
    /* NOTE: the L2 baseline must be taken **before** send. Taking it afterwards folds this
     * command's own acknowledgement into the baseline, so
     * cmdack_now == cmdack_at_send would hold forever -> stuck in SrWaitStageCmd forever. */
    app->scan_cmdack_at_send =
        sr_worker_cmdack_count(app->worker, is_start ? SrCmdAckStart : SrCmdAckStop);
    if(n > 0 && sr_worker_send_cmd(app->worker, cmd)) {
        app->scan.cmd_pending = true;
        app->scan.cmd_rejected = false;
    } else {
        /* Slot occupied (or send otherwise failed): keep cmd_pending so the
         * in-flight command's ack criterion is not erased (D12 A2). */
        app->scan.cmd_rejected = true;
    }
}

/* OK-key dispatch via sr_scan_ctl_on_ok (D12 A1). State is eval'd over a
 * stack copy, same convention as dash_fill. */
static void dash_scan_toggle(SigRoamApp* app) {
    SrScanCtlCtx ctx;
    SrScanUiState st;
    SrScanAct act;

    ctx = app->scan;
    ctx.session_rev_now = app->model.session_rev;
    ctx.session_now = app->model.session;
    st = sr_scan_ctl_eval(&ctx, furi_get_tick());
    act = sr_scan_ctl_on_ok(st);
    if(act == SrScanActSendStart) {
        dash_queue_cmd(app, true);
    } else if(act == SrScanActSendStop) {
        dash_queue_cmd(app, false);
    }
}

/* Moved from scene_drive.c:191-197. Command fulfilled -> clear pending, and the UI returns
 * from "command in progress" to its normal state.
 * A timeout does **not** clear pending -- this is the core fix of this task card:
 *    sr_scan_ctl_eval returns StartFailed after the timeout, but cmd_pending stays true,
 *    so the UI keeps showing what it is waiting for instead of declaring failure, until
 *    session_rev actually increments.
 *    V-061 (3) measured the device entering running by itself after 40 s -- the code was
 *    always going to recover on its own.
 * Returns true when the pending state changed, so the caller refreshes the snapshot again. */
static bool dash_scan_tick(SigRoamApp* app) {
    SrScanCtlCtx ctx;
    SrScanUiState st;

    if(!app->scan.cmd_pending) {
        return false;
    }
    ctx = app->scan;
    ctx.session_rev_now = app->model.session_rev;
    ctx.session_now = app->model.session;
    st = sr_scan_ctl_eval(&ctx, furi_get_tick());
    if((st == SrScanUiRunning && app->scan.cmd_is_start) ||
       (st == SrScanUiIdle && !app->scan.cmd_is_start)) {
        app->scan.cmd_pending = false;
        app->scan.cmd_rejected = false; /* D12 A4: reject latched while pending is stale */
        return true;
    }
    return false;
}

void sigroam_scene_dash_on_enter(void* context) {
    SigRoamApp* app = context;

    /* Entered via the custom callback, so app->mtx is already held. Do not acquire again. No blocking IO. */
    sr_view_dash_set_callback(app->dash, dash_view_scroll_cb, app);
    sr_view_dash_set_ok_callback(app->dash, dash_view_ok_cb, app);
    sigroam_dash_refresh(app);
    sr_notify_backlight_enforce(app->notify, sr_settings_effective_backlight(&app->settings));
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewDash);
}

bool sigroam_scene_dash_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    /* app->mtx is already held (control only reaches here when the sigroam.c custom callback took it with zero wait). Do not acquire again. */
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SigRoamDashEventScroll) {
            sigroam_dash_refresh(app);
            return true;
        }
        if(event.event == SigRoamDashEventOk) {
            uint8_t tab = (uint8_t)SR_VIEW_TAB_DASH;
            View* v = sr_view_dash_get_view(app->dash);
            if(v != NULL) {
                with_view_model(
                    v, SrDashModel* cur, { if(cur != NULL) { tab = cur->tab; } }, false);
            }
            if(tab == (uint8_t)SR_VIEW_TAB_GPS) {
                const SrSourceCodec* codec;
                SrScanCtlCtx ctx;
                uint8_t scan_ui;
                bool serial_open;
                bool poi_link;
                bool fix;
                SrPoiGate g;

                serial_open = (app->io != NULL && sr_io_is_open(app->io));
                codec = sigroam_codec(app);
                poi_link = serial_open && codec != NULL && codec->build_poi_cmd != NULL;
                ctx = app->scan;
                ctx.session_rev_now = app->model.session_rev;
                ctx.session_now = app->model.session;
                scan_ui = (uint8_t)sr_scan_ctl_eval(&ctx, furi_get_tick());
                fix = false;
                if(app->model.session == SrSessionRunning && app->model.gps_csv_rev > 0) {
                    fix = app->model.gps_csv.fix;
                } else if(app->model.gps_blocks > 0) {
                    fix = app->model.gps.fix;
                }
                g = sr_poi_gate(
                    poi_link, (uint8_t)app->model.session, scan_ui, fix);
                if(g == SrPoiGateNotScanning) {
                    dash_gps_start(app);
                } else if(g == SrPoiGateOk) {
                    dash_poi_send(app);
                }
            } else if(tab == (uint8_t)SR_VIEW_TAB_DASH) {
                dash_scan_toggle(app);
            }
            sigroam_dash_refresh(app);
            return true;
        }
        return false;
    }

    if(event.type == SceneManagerEventTypeTick) {
        bool need = dash_gps_tick(app);
        if(dash_scan_tick(app)) {
            need = true;
        }
        if(dash_poi_tick(app)) {
            need = true;
        }
        SrAlertKind ak = sr_alert_eval(
            &app->alert, app->model.gps_csv_rev, app->model.gps_csv.fix, furi_get_tick());
        if(ak != SrAlertNone) {
            sr_notify_alert(app->notify, ak, &app->settings);
        }
        if(need) {
            sigroam_dash_refresh(app);
        }
        return true;
    }

    return false;
}

void sigroam_scene_dash_on_exit(void* context) {
    SigRoamApp* app = context;

    /* If one is in flight, send the close-out command immediately (ADR-020 decision 3), then unregister the callback. */
    dash_gps_abort(app);
    sr_notify_backlight_enforce(app->notify, false); /* Decision 7: always restore */
    if(app != NULL && app->dash != NULL) {
        sr_view_dash_set_callback(app->dash, NULL, NULL);
        sr_view_dash_set_ok_callback(app->dash, NULL, NULL);
    }
}
