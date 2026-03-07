/**
 * rogue_ap.c — Rogue AP Detector: main application, UI, and entry point.
 *
 * View hierarchy
 * ~~~~~~~~~~~~~~
 *   RogueViewMainMenu  Submenu           — root: Scan / Results / Settings
 *   RogueViewScan      View (custom)     — live threat status + AP count
 *   RogueViewResults   TextBox           — all flagged SSIDs + BSSIDs
 *   RogueViewSettings  VariableItemList  — min-RSSI filter
 *
 * Threat detection runs on the UART worker thread (rogue_ap_worker.c).
 * A 500 ms FuriTimer copies results into the scan view model and triggers
 * a canvas redraw.  All RogueApResults access is mutex-protected.
 */

#include "rogue_ap.h"

#include <stdio.h>
#include <string.h>

#define TAG "RogueAP"

/* =========================================================================
 * Forward declarations
 * ========================================================================= */

static bool rogue_nav_callback(void* ctx);
static bool rogue_custom_event_callback(void* ctx, uint32_t event);

static void rogue_main_menu_callback(void* ctx, uint32_t index);
static void rogue_scan_draw_callback(Canvas* canvas, void* model);
static bool rogue_scan_input_callback(InputEvent* event, void* ctx);
static void rogue_refresh_timer_cb(void* ctx);
static void rogue_rebuild_results(RogueApp* app);
static void rogue_settings_setup(RogueApp* app);

/* =========================================================================
 * Custom events
 * ========================================================================= */

typedef enum {
    RogueCustomEventThreatDetected = 0,
} RogueCustomEvent;

/* =========================================================================
 * Scan view — draw callback
 *
 * Layout (128 x 64):
 *   Row 0-12:  app title + ESP32 connection indicator
 *   Row 13:    separator
 *   Row 14-30: status block (CLEAN / SUSPECT / EVIL TWIN) with indicator glyph
 *   Row 31-45: AP count
 *   Row 46-63: flagged SSID summary (wraps if long)
 *
 * Rendered entirely from the locked view model — no mutex needed here.
 * ========================================================================= */
static void rogue_scan_draw_callback(Canvas* canvas, void* model) {
    const RogueScanModel* m = (const RogueScanModel*)model;

    canvas_clear(canvas);

    /* ---- Header ---- */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "Rogue AP Detector");

    canvas_set_font(canvas, FontSecondary);
    const char* conn_str = m->scanning ? "[live]" : "[idle]";
    canvas_draw_str(canvas, 88, 11, conn_str);

    canvas_draw_line(canvas, 0, 13, 127, 13);

    /* ---- Status block ---- */
    canvas_set_font(canvas, FontPrimary);

    /* Status indicator glyph: filled box (threat) or outline box (clean). */
    if(m->status == RogueStatusClean) {
        canvas_draw_frame(canvas, 2, 17, 8, 8);
        canvas_draw_str(canvas, 14, 25, "CLEAN");
    } else if(m->status == RogueStatusSuspect) {
        canvas_draw_box(canvas, 2, 17, 8, 8);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_dot(canvas, 5, 20);
        canvas_draw_dot(canvas, 6, 20);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 14, 25, "SUSPECT");
    } else {
        /* EVIL_TWIN — invert block to make it visually alarming. */
        canvas_draw_box(canvas, 0, 15, 128, 12);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 2, 25, "!! EVIL TWIN !!");
        canvas_set_color(canvas, ColorBlack);
    }

    /* ---- AP count ---- */
    canvas_set_font(canvas, FontSecondary);
    char ap_line[24];
    snprintf(ap_line, sizeof(ap_line), "APs seen: %lu", (unsigned long)m->ap_count);
    canvas_draw_str(canvas, 2, 37, ap_line);

    /* ---- Flagged SSID summary ---- */
    if(m->status != RogueStatusClean && m->flagged_ssid[0] != '\0') {
        /* Truncate long SSIDs to fit the display width. */
        char ssid_disp[20];
        strncpy(ssid_disp, m->flagged_ssid, sizeof(ssid_disp) - 1);
        ssid_disp[sizeof(ssid_disp) - 1] = '\0';

        /* "ssid_disp (NNN BSSIDs)": 19 + 1 + 13 = 33 max → 40 is safe. */
        char bssid_line[40];
        snprintf(
            bssid_line,
            sizeof(bssid_line),
            "%s (%lu BSSIDs)",
            ssid_disp,
            (unsigned long)m->flagged_bssid_count);
        canvas_draw_str(canvas, 2, 48, bssid_line);
        canvas_draw_str(canvas, 2, 60, "See Results for details");
    } else if(m->status == RogueStatusClean) {
        canvas_draw_str(canvas, 2, 48, "No threats detected");
        canvas_draw_str(canvas, 2, 60, m->scanning ? "Monitoring..." : "Press OK to scan");
    }
}

/* =========================================================================
 * Scan view — input callback
 *
 * OK: toggle scanning on/off.
 * ========================================================================= */
static bool rogue_scan_input_callback(InputEvent* event, void* ctx) {
    RogueApp* app = (RogueApp*)ctx;

    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        if(rogue_ap_worker_is_scanning(app->worker)) {
            rogue_ap_worker_stop(app->worker);
            with_view_model(app->scan_view, RogueScanModel * m, { m->scanning = false; }, true);
        } else {
            /* Clear stale results before a fresh scan. */
            furi_mutex_acquire(app->ap_results->mutex, FuriWaitForever);
            app->ap_results->ap_count = 0;
            app->ap_results->overall_status = RogueStatusClean;
            app->ap_results->flagged_ssid[0] = '\0';
            app->ap_results->flagged_bssid_count = 0;
            furi_mutex_release(app->ap_results->mutex);

            app->notified_this_threat = false;
            rogue_ap_worker_start(app->worker);
            with_view_model(app->scan_view, RogueScanModel * m, { m->scanning = true; }, true);
        }
        return true;
    }

    return false;
}

/* =========================================================================
 * Refresh timer callback — fires every 500 ms.
 *
 * Copies a snapshot of RogueApResults into the scan view model and
 * triggers a canvas redraw.  Also sends a custom event when a new threat
 * is detected so the main thread can fire notifications.
 * ========================================================================= */
static void rogue_refresh_timer_cb(void* ctx) {
    RogueApp* app = (RogueApp*)ctx;

    if(furi_mutex_acquire(app->ap_results->mutex, furi_ms_to_ticks(10)) != FuriStatusOk) {
        return;
    }

    RogueStatus status = app->ap_results->overall_status;
    uint32_t ap_count = app->ap_results->ap_count;
    char flagged_ssid[ROGUE_SSID_LEN];
    uint32_t flagged_bssid_count = app->ap_results->flagged_bssid_count;

    strncpy(flagged_ssid, app->ap_results->flagged_ssid, ROGUE_SSID_LEN - 1);
    flagged_ssid[ROGUE_SSID_LEN - 1] = '\0';

    furi_mutex_release(app->ap_results->mutex);

    /* Push snapshot into the scan view model and request redraw. */
    with_view_model(
        app->scan_view,
        RogueScanModel * m,
        {
            m->status = status;
            m->ap_count = ap_count;
            m->flagged_bssid_count = flagged_bssid_count;
            strncpy(m->flagged_ssid, flagged_ssid, ROGUE_SSID_LEN - 1);
            m->flagged_ssid[ROGUE_SSID_LEN - 1] = '\0';
        },
        true);

    /* Notify once per new threat escalation. */
    if(status >= RogueStatusSuspect && !app->notified_this_threat) {
        app->notified_this_threat = true;
        view_dispatcher_send_custom_event(app->view_dispatcher, RogueCustomEventThreatDetected);
    } else if(status == RogueStatusClean) {
        /* Reset so we fire again if a threat appears later. */
        app->notified_this_threat = false;
    }
}

/* =========================================================================
 * Custom event callback — runs on the GUI thread.
 * ========================================================================= */
static bool rogue_custom_event_callback(void* ctx, uint32_t event) {
    RogueApp* app = (RogueApp*)ctx;

    if((RogueCustomEvent)event == RogueCustomEventThreatDetected) {
        /* Re-read status to decide alert severity. */
        furi_mutex_acquire(app->ap_results->mutex, FuriWaitForever);
        RogueStatus status = app->ap_results->overall_status;
        furi_mutex_release(app->ap_results->mutex);

        if(status == RogueStatusEvilTwin) {
            notification_message(app->notifications, &sequence_blink_red_100);
            notification_message(app->notifications, &sequence_single_vibro);
        } else {
            /* SUSPECT — gentler alert: single short blink. */
            notification_message(app->notifications, &sequence_blink_yellow_100);
        }
        return true;
    }

    return false;
}

/* =========================================================================
 * Results TextBox — rebuilt on demand.
 *
 * Lists every flagged SSID and each BSSID seen for it.
 * ========================================================================= */
static void rogue_rebuild_results(RogueApp* app) {
    furi_string_reset(app->results_text);

    furi_mutex_acquire(app->ap_results->mutex, FuriWaitForever);

    RogueStatus overall = app->ap_results->overall_status;

    if(overall == RogueStatusClean) {
        furi_string_cat_printf(app->results_text, "No threats detected.\n");
        furi_string_cat_printf(
            app->results_text, "Total APs: %lu\n", (unsigned long)app->ap_results->ap_count);
    } else {
        /* Collect all SSIDs that have 2+ BSSIDs and list them. */
        /* To avoid dynamic allocation we do two passes: first collect
     * unique SSIDs, then for each print its BSSIDs. */

        /* We need a small unique-SSID list — stack-allocate since SSID_LEN
     * is small and ROGUE_MAX_APS is bounded.  Worst case is 128 * 33 =
     * 4224 bytes — too large for the stack.  Use the results text buffer
     * as a scratchpad instead via repeated scans. */

        /* Pass 1: count total flagged SSIDs (unique). */
        uint32_t flagged_count = 0;

        for(uint32_t i = 0; i < app->ap_results->ap_count; i++) {
            const char* ssid = app->ap_results->aps[i].ssid;

            /* Check if this SSID was already counted (first occurrence). */
            bool already = false;
            for(uint32_t k = 0; k < i; k++) {
                if(strcmp(app->ap_results->aps[k].ssid, ssid) == 0) {
                    already = true;
                    break;
                }
            }
            if(already) continue;

            /* Count distinct BSSIDs for this SSID. */
            uint32_t bssid_count = 0;
            for(uint32_t j = 0; j < app->ap_results->ap_count; j++) {
                if(strcmp(app->ap_results->aps[j].ssid, ssid) == 0) bssid_count++;
            }

            if(bssid_count < 2) continue;

            flagged_count++;
            /* Classify this SSID. */
            int8_t max_rssi = -127, min_rssi = 0;
            bool min_set = false;
            for(uint32_t j = 0; j < app->ap_results->ap_count; j++) {
                if(strcmp(app->ap_results->aps[j].ssid, ssid) != 0) continue;
                if(app->ap_results->aps[j].rssi > max_rssi)
                    max_rssi = app->ap_results->aps[j].rssi;
                if(!min_set || app->ap_results->aps[j].rssi < min_rssi) {
                    min_rssi = app->ap_results->aps[j].rssi;
                    min_set = true;
                }
            }
            int8_t delta = (int8_t)(max_rssi - min_rssi);
            const char* label = (delta >= ROGUE_EVIL_TWIN_RSSI_DELTA) ? "[EVIL TWIN]" :
                                                                        "[SUSPECT]";

            furi_string_cat_printf(app->results_text, "%s %.32s\n", label, ssid);

            /* List each BSSID with its RSSI and channel. */
            for(uint32_t j = 0; j < app->ap_results->ap_count; j++) {
                if(strcmp(app->ap_results->aps[j].ssid, ssid) != 0) continue;
                furi_string_cat_printf(
                    app->results_text,
                    "  %s ch%u %ddBm\n",
                    app->ap_results->aps[j].bssid,
                    (unsigned)app->ap_results->aps[j].channel,
                    (int)app->ap_results->aps[j].rssi);
            }
        }

        if(flagged_count == 0) {
            furi_string_cat_printf(app->results_text, "No flagged SSIDs.\n");
        }

        furi_string_cat_printf(
            app->results_text, "\nTotal APs: %lu\n", (unsigned long)app->ap_results->ap_count);
    }

    furi_mutex_release(app->ap_results->mutex);

    text_box_reset(app->results_box);
    text_box_set_text(app->results_box, furi_string_get_cstr(app->results_text));
}

/* =========================================================================
 * Main menu
 * ========================================================================= */

typedef enum {
    RogueMainMenuScan = 0,
    RogueMainMenuResults,
    RogueMainMenuSettings,
} RogueMainMenuItem;

static void rogue_main_menu_callback(void* ctx, uint32_t index) {
    RogueApp* app = (RogueApp*)ctx;

    switch((RogueMainMenuItem)index) {
    case RogueMainMenuScan:
        app->current_view = RogueViewScan;
        view_dispatcher_switch_to_view(app->view_dispatcher, RogueViewScan);
        break;

    case RogueMainMenuResults:
        rogue_rebuild_results(app);
        app->current_view = RogueViewResults;
        view_dispatcher_switch_to_view(app->view_dispatcher, RogueViewResults);
        break;

    case RogueMainMenuSettings:
        app->current_view = RogueViewSettings;
        view_dispatcher_switch_to_view(app->view_dispatcher, RogueViewSettings);
        break;
    }
}

/* =========================================================================
 * Settings view
 *
 * Single item: "Min RSSI" with values -50, -60, -70, -80, -90 (dBm).
 * ========================================================================= */

static const int8_t rssi_options[] = {-50, -60, -70, -80, -90};
static const char* rssi_option_labels[] = {"-50", "-60", "-70", "-80", "-90"};
#define RSSI_OPTIONS_COUNT 5

static void rogue_rssi_change_cb(VariableItem* item) {
    RogueApp* app = (RogueApp*)variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= RSSI_OPTIONS_COUNT) idx = RSSI_OPTIONS_COUNT - 1;
    variable_item_set_current_value_text(item, rssi_option_labels[idx]);
    app->settings.min_rssi = rssi_options[idx];
}

static void rogue_settings_setup(RogueApp* app) {
    variable_item_list_reset(app->settings_list);

    VariableItem* rssi_item = variable_item_list_add(
        app->settings_list, "Min RSSI (dBm)", RSSI_OPTIONS_COUNT, rogue_rssi_change_cb, app);

    /* Find and select the current setting. */
    uint8_t sel = RSSI_OPTIONS_COUNT - 1; /* default: -90 */
    for(uint8_t i = 0; i < RSSI_OPTIONS_COUNT; i++) {
        if(rssi_options[i] == app->settings.min_rssi) {
            sel = i;
            break;
        }
    }
    variable_item_set_current_value_index(rssi_item, sel);
    variable_item_set_current_value_text(rssi_item, rssi_option_labels[sel]);
}

/* =========================================================================
 * Navigation callback
 * ========================================================================= */
static bool rogue_nav_callback(void* ctx) {
    RogueApp* app = (RogueApp*)ctx;

    switch(app->current_view) {
    case RogueViewScan:
        /* Stop scanning when leaving the scan view. */
        if(rogue_ap_worker_is_scanning(app->worker)) {
            rogue_ap_worker_stop(app->worker);
        }
        app->current_view = RogueViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, RogueViewMainMenu);
        return true;

    case RogueViewResults:
        app->current_view = RogueViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, RogueViewMainMenu);
        return true;

    case RogueViewSettings:
        app->current_view = RogueViewMainMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, RogueViewMainMenu);
        return true;

    case RogueViewMainMenu:
    default:
        view_dispatcher_stop(app->view_dispatcher);
        return false;
    }
}

/* =========================================================================
 * App alloc
 * ========================================================================= */
static RogueApp* rogue_app_alloc(void) {
    RogueApp* app = malloc(sizeof(RogueApp));
    furi_assert(app);
    memset(app, 0, sizeof(RogueApp));

    app->current_view = RogueViewMainMenu;
    app->settings.min_rssi = -90;

    /* ---- Service records ---- */
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    /* ---- AP results table ---- */
    app->ap_results = malloc(sizeof(RogueApResults));
    furi_assert(app->ap_results);
    memset(app->ap_results, 0, sizeof(RogueApResults));
    app->ap_results->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(app->ap_results->mutex);

    /* ---- UART + worker ---- */
    app->uart = rogue_uart_alloc();
    app->worker = rogue_ap_worker_alloc(app->uart, app->ap_results);

    /* ---- View dispatcher ---- */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, rogue_nav_callback);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, rogue_custom_event_callback);

    /* ---- Main menu ---- */
    app->main_menu = submenu_alloc();
    submenu_set_header(app->main_menu, "Rogue AP Detector");
    submenu_add_item(app->main_menu, "Scan", RogueMainMenuScan, rogue_main_menu_callback, app);
    submenu_add_item(
        app->main_menu, "Results", RogueMainMenuResults, rogue_main_menu_callback, app);
    submenu_add_item(
        app->main_menu, "Settings", RogueMainMenuSettings, rogue_main_menu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, RogueViewMainMenu, submenu_get_view(app->main_menu));

    /* ---- Scan view ---- */
    app->scan_view = view_alloc();
    view_set_context(app->scan_view, app);
    view_set_draw_callback(app->scan_view, rogue_scan_draw_callback);
    view_set_input_callback(app->scan_view, rogue_scan_input_callback);
    view_allocate_model(app->scan_view, ViewModelTypeLocking, sizeof(RogueScanModel));
    with_view_model(
        app->scan_view, RogueScanModel * m, { memset(m, 0, sizeof(RogueScanModel)); }, false);
    view_dispatcher_add_view(app->view_dispatcher, RogueViewScan, app->scan_view);

    /* ---- Results TextBox ---- */
    app->results_text = furi_string_alloc();
    app->results_box = text_box_alloc();
    text_box_set_font(app->results_box, TextBoxFontText);
    text_box_set_focus(app->results_box, TextBoxFocusStart);
    view_dispatcher_add_view(
        app->view_dispatcher, RogueViewResults, text_box_get_view(app->results_box));

    /* ---- Settings view ---- */
    app->settings_list = variable_item_list_alloc();
    rogue_settings_setup(app);
    view_dispatcher_add_view(
        app->view_dispatcher, RogueViewSettings, variable_item_list_get_view(app->settings_list));

    /* ---- Refresh timer (500 ms periodic) ---- */
    app->refresh_timer = furi_timer_alloc(rogue_refresh_timer_cb, FuriTimerTypePeriodic, app);
    furi_timer_start(app->refresh_timer, furi_ms_to_ticks(500));

    return app;
}

/* =========================================================================
 * App free
 * ========================================================================= */
static void rogue_app_free(RogueApp* app) {
    furi_assert(app);

    /* Stop the refresh timer before anything else. */
    furi_timer_stop(app->refresh_timer);
    furi_timer_free(app->refresh_timer);
    app->refresh_timer = NULL;

    /* Stop scanning cleanly. */
    if(rogue_ap_worker_is_scanning(app->worker)) {
        rogue_ap_worker_stop(app->worker);
    }
    rogue_ap_worker_free(app->worker);
    rogue_uart_free(app->uart);
    app->worker = NULL;
    app->uart = NULL;

    /* Remove views before freeing underlying objects. */
    view_dispatcher_remove_view(app->view_dispatcher, RogueViewMainMenu);
    view_dispatcher_remove_view(app->view_dispatcher, RogueViewScan);
    view_dispatcher_remove_view(app->view_dispatcher, RogueViewResults);
    view_dispatcher_remove_view(app->view_dispatcher, RogueViewSettings);

    view_dispatcher_free(app->view_dispatcher);

    submenu_free(app->main_menu);
    view_free(app->scan_view);
    text_box_free(app->results_box);
    furi_string_free(app->results_text);
    variable_item_list_free(app->settings_list);

    /* Free results table last (worker may have been referencing it). */
    furi_mutex_free(app->ap_results->mutex);
    free(app->ap_results);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

/* =========================================================================
 * Entry point
 * ========================================================================= */
int32_t rogue_ap_app(void* p) {
    UNUSED(p);

    RogueApp* app = rogue_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, RogueViewMainMenu);

    /* Blocks until view_dispatcher_stop() is called. */
    view_dispatcher_run(app->view_dispatcher);

    rogue_app_free(app);

    return 0;
}
