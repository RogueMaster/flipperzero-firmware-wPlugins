/**
 * rayhunter_worker.c — ESP32 poll and response parser for Ray Hunter Client.
 *
 * Protocol
 * ~~~~~~~~
 * The Flipper sends "rayhunter_poll\n" to the ESP32.  The ESP32 (running a
 * companion sketch that proxies HTTP GET /api/analysis-report/live) replies
 * with one or more NDJSON lines.  We parse each line for:
 *
 *   Threat keywords  →  update RhThreatLevel
 *   Alert snippets   →  capture last_message
 *   Status keywords  →  update status_text
 *
 * Thread safety
 * ~~~~~~~~~~~~~
 * rh_worker_rx_line() fires on the UART worker thread.  It writes into the
 * main view model under the model's internal locking mutex (via
 * with_view_model) and posts a custom event — both are documented as safe
 * from any thread.
 *
 * The config is read under config_mutex (acquired briefly, copied).
 */

#include "rayhunter_worker.h"
#include "rayhunter_uart.h"

#include <stdio.h>
#include <string.h>
#include <strings.h> /* strcasestr */

#define TAG "RhWorker"

/* --------------------------------------------------------------------------
 * Threat keyword table
 *
 * Ordered from highest severity to lowest so the first match wins when
 * multiple keywords appear on one line.
 * -------------------------------------------------------------------------- */
typedef struct {
    const char* keyword;
    RhThreatLevel level;
} RhKeyword;

static const RhKeyword kw_threat[] = {
    {"\"High\"", RhThreatHigh},
    {"\"Medium\"", RhThreatMedium},
    {"\"Low\"", RhThreatLow},
    {"\"Informational\"", RhThreatClean},
    {"clean", RhThreatClean},
    {NULL, RhThreatNone},
};

/* Alert message substrings to capture verbatim. */
static const char* kw_alert[] = {
    "null cipher",
    "Identity requested",
    "Cell suggested",
    "IMSI",
    "suspicious manner",
    NULL,
};

/* Status substrings for the status_text field. */
static const char* kw_status_running[] = {"recording", "running", "active"};
static const char* kw_status_idle[] = {"idle", "finished", "queued"};

/* --------------------------------------------------------------------------
 * Helper: case-insensitive substring search (POSIX strcasestr)
 * -------------------------------------------------------------------------- */
static const char* rh_strstr_ci(const char* haystack, const char* needle) {
    return strcasestr(haystack, needle);
}

/* --------------------------------------------------------------------------
 * Parse one line received from the ESP32.
 *
 * Called on the UART worker thread.  Writes model under view model lock.
 * Posts RhEventStatusUpdate (always) and RhEventHighThreat (when High).
 * -------------------------------------------------------------------------- */
static void rh_worker_rx_line(const char* line, void* ctx) {
    RhApp* app = (RhApp*)ctx;

    if(!line || line[0] == '\0') return;

    FURI_LOG_D(TAG, "RX: %s", line);

    /* --- 1. Determine threat level from the line. --- */
    RhThreatLevel parsed_threat = RhThreatNone;

    for(int i = 0; kw_threat[i].keyword != NULL; i++) {
        if(strstr(line, kw_threat[i].keyword) != NULL) {
            parsed_threat = kw_threat[i].level;
            break; /* highest severity wins */
        }
    }

    /* --- 2. Check for alert messages to surface as last_message. --- */
    char alert_msg[RH_MESSAGE_LEN] = {0};
    for(int i = 0; kw_alert[i] != NULL; i++) {
        if(rh_strstr_ci(line, kw_alert[i]) != NULL) {
            /* Use the keyword itself as a short label. */
            snprintf(alert_msg, sizeof(alert_msg), "%s", kw_alert[i]);
            break;
        }
    }

    /* --- 3. Infer recording status from status keywords. --- */
    char status_buf[RH_STATUS_LEN] = {0};
    for(size_t i = 0; i < sizeof(kw_status_running) / sizeof(kw_status_running[0]); i++) {
        if(rh_strstr_ci(line, kw_status_running[i]) != NULL) {
            snprintf(status_buf, sizeof(status_buf), "Recording: active");
            break;
        }
    }
    if(status_buf[0] == '\0') {
        for(size_t i = 0; i < sizeof(kw_status_idle) / sizeof(kw_status_idle[0]); i++) {
            if(rh_strstr_ci(line, kw_status_idle[i]) != NULL) {
                snprintf(status_buf, sizeof(status_buf), "Idle");
                break;
            }
        }
    }

    /* If the line starts with "{" it's JSON — mark connected. */
    bool is_json = (line[0] == '{');

    /* --- 4. Push into the main view model (locked). --- */
    with_view_model(
        app->main_view,
        RhMainModel * m,
        {
            /* Ratchet threat upward within a poll window; reset on next poll
         * is handled by rh_worker_poll() zeroing out packet_count guard. */
            if(parsed_threat > m->status.threat) {
                m->status.threat = parsed_threat;
            }

            if(alert_msg[0] != '\0') {
                strncpy(m->status.last_message, alert_msg, RH_MESSAGE_LEN - 1);
                m->status.last_message[RH_MESSAGE_LEN - 1] = '\0';
                m->status.warning_count++;
            }

            if(status_buf[0] != '\0') {
                strncpy(m->status.status_text, status_buf, RH_STATUS_LEN - 1);
                m->status.status_text[RH_STATUS_LEN - 1] = '\0';
            }

            if(is_json) {
                m->status.packet_count++;
            }

            m->status.connected = rh_uart_is_connected(app->uart);
            m->status.last_update_tick = furi_get_tick();
        },
        true /* update = true triggers canvas redraw */
    );

    /* --- 5. Post events to the GUI thread. --- */
    view_dispatcher_send_custom_event(app->view_dispatcher, RhEventStatusUpdate);

    if(parsed_threat == RhThreatHigh && app->last_notified_threat != RhThreatHigh) {
        app->last_notified_threat = RhThreatHigh;
        view_dispatcher_send_custom_event(app->view_dispatcher, RhEventHighThreat);
    } else if(parsed_threat < RhThreatHigh) {
        /* Allow re-notification when threat falls and then rises again. */
        app->last_notified_threat = parsed_threat;
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

RhApp* rh_worker_start(RhApp* app) {
    furi_assert(app);
    furi_assert(app->uart);

    rh_uart_set_rx_callback(app->uart, rh_worker_rx_line, app);

    FURI_LOG_I(TAG, "Worker started");
    return app;
}

void rh_worker_stop(RhApp* app) {
    furi_assert(app);

    /* Unregister the callback first so no more lines are dispatched. */
    rh_uart_set_rx_callback(app->uart, NULL, NULL);

    FURI_LOG_I(TAG, "Worker stopped");
}

void rh_worker_poll(RhApp* app) {
    furi_assert(app);

    if(!app->uart) return;

    /* Before sending a fresh poll, reset the threat level in the model so
   * stale data from previous responses doesn't linger indefinitely.
   * We preserve packet_count and warning_count as running totals. */
    with_view_model(
        app->main_view,
        RhMainModel * m,
        {
            m->status.threat = RhThreatNone;
            /* Clear last_message so "clean" polls show nothing. */
            if(m->status.threat == RhThreatNone) {
                m->status.last_message[0] = '\0';
            }
        },
        false /* no redraw needed for the reset itself */
    );

    /* Send the poll command to the ESP32. */
    rh_uart_send(app->uart, "rayhunter_poll");

    FURI_LOG_D(TAG, "Poll sent");
}
