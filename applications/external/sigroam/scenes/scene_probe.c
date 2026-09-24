#include "../sigroam.h"
#include "../src/sr_dialect.h"
#include "../src/sr_probe_fmt.h"

#include <stdio.h>
#include <string.h>

static void sigroam_scene_probe_fill(SigRoamApp* app, SrHandshakeState st) {
    const SrFirmwareInfo* fw = &app->model.firmware;

    switch(st) {
    case SrHandshakeIdle:
        if(app->probe_send_busy) {
            snprintf(
                app->probe_text,
                sizeof(app->probe_text),
                "\e#Command busy\n"
                "\n"
                "previous command\n"
                "not sent yet, retry");
        } else {
            snprintf(
                app->probe_text,
                sizeof(app->probe_text),
                "\e#Not connected\n"
                "%s\n"
                "\n"
                "Need Scout Lite\n"
                "running SigRoam\n"
                "on GPIO 13/14,\n"
                "5V on pin 1.\n"
                "Check wiring and\n"
                "Log Device Off.\n"
                "\n"
                "Then exit and\n"
                "reopen this app:\n"
                "the serial port is\n"
                "opened only at\n"
                "startup.",
                sigroam_io_status_hint(app->io_status));
        }
        break;
    case SrHandshakeWaiting:
        if(app->hs.sends >= 2u) {
            snprintf(
                app->probe_text,
                sizeof(app->probe_text),
                "\e#Probing...\n"
                "\n"
                "Retrying info\n"
                "Waiting up to 1.5s");
        } else {
            snprintf(
                app->probe_text,
                sizeof(app->probe_text),
                "\e#Probing...\n"
                "\n"
                "Sent: info\n"
                "Waiting up to 1.5s");
        }
        break;
    case SrHandshakeOk:
        /* ADR-027: product identity, not UART Version: / ESP-IDF:. Diag stays. */
        (void)sr_probe_fmt_ok(
            fw, SR_SCANNER_VERSION, SR_BRAND_LINE, app->probe_text, sizeof(app->probe_text));
        break;
    case SrHandshakeUnknownFw:
        snprintf(
            app->probe_text,
            sizeof(app->probe_text),
            "\e#Unknown firmware\n"
            "\n"
            "Replied, but not\n"
            "Marauder.\n"
            "Got: %s\n"
            "\n"
            "Check the adapter\n"
            "firmware.",
            fw->firmware[0] != '\0' ? fw->firmware : "(no name)");
        break;
    case SrHandshakeNoReply:
        /* Keep listening: a late Firmware: still lifts eval to Ok. Do not
         * paint a terminal No reply that then jumps to the firmware page. */
        snprintf(
            app->probe_text,
            sizeof(app->probe_text),
            "\e#Probing...\n"
            "\n"
            "Board may still be\n"
            "booting. Waiting.\n"
            "\n"
            "Back to leave.\n"
            "Else pin 13/14, 5V,\n"
            "and baud (%lu).",
            (unsigned long)app->settings.baud);
        break;
    default:
        app->probe_text[0] = '\0';
        break;
    }
}

static void sigroam_scene_probe_draw(SigRoamApp* app, SrHandshakeState st) {
    sigroam_scene_probe_fill(app, st);
    widget_reset(app->widget);
    /* Do not take the return of widget_add_text_box_element: Official is
     * void, Momentum is WidgetElement*. text_scroll is void on both. */
    widget_add_text_scroll_element(app->widget, 0, 0, SR_CANVAS_W, SR_CANVAS_H, app->probe_text);
    app->hs_shown = st;
    app->hs_rev_shown = app->model.firmware_rev;
}

/* Queue `info\n` and start a new 1.5s window. On failure leave ctx untouched
 * so a retry that hits a full worker slot still evaluates as NoReply. */
static bool probe_queue_info(SigRoamApp* app) {
    SrIoStats stats;

    if(app == NULL || app->io == NULL || !sr_io_is_open(app->io) || app->worker == NULL) {
        return false;
    }
    if(!sr_worker_send_cmd(app->worker, "info\n")) {
        return false;
    }
    sr_io_get_stats(app->io, &stats);
    app->hs.rx_bytes_at_send = stats.rx_bytes;
    app->hs.rx_bytes_now = stats.rx_bytes;
    app->hs.fw_rev_at_send = app->model.firmware_rev;
    app->hs.fw_rev_now = app->model.firmware_rev;
    app->hs.fw_kind = app->model.firmware.kind;
    app->hs.sent_tick_ms = furi_get_tick();
    app->hs.sent = true;
    if(app->hs.sends < 255u) {
        app->hs.sends++;
    }
    return true;
}

/* Generic Marauder only. Does not touch handshake ctx. Single worker slot. */
static bool probe_queue_stopscan(SigRoamApp* app) {
    const SrSourceCodec* codec;
    char cmd[SR_WORKER_CMD_MAX];
    size_t n;

    if(app == NULL || app->io == NULL || !sr_io_is_open(app->io) || app->worker == NULL) {
        return false;
    }
    codec = sigroam_codec(app);
    if(codec == NULL || codec->build_stop_cmd == NULL) {
        return false;
    }
    n = codec->build_stop_cmd(cmd, sizeof(cmd));
    if(n == 0u) {
        return false;
    }
    return sr_worker_send_cmd(app->worker, cmd);
}

void sigroam_scene_probe_on_enter(void* context) {
    SigRoamApp* app = context;
    SrHandshakeState st;

    memset(&app->hs, 0, sizeof(app->hs));
    app->hs.timeout_ms = SR_HANDSHAKE_TIMEOUT_MS;
    app->probe_send_busy = false;
    app->probe_stop_sent = false;

    if(app->io && sr_io_is_open(app->io) && app->worker) {
        app->probe_send_busy = !probe_queue_info(app);
    } else {
        /* Fill this in even when io is not open; otherwise the memset leaves 0 and, once tick
         * refreshes now, it would be misjudged as Ok. */
        app->hs.fw_rev_at_send = app->model.firmware_rev;
        app->hs.fw_rev_now = app->model.firmware_rev;
    }

    st = sr_handshake_eval(&app->hs, furi_get_tick());
    sigroam_scene_probe_draw(app, st);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewWidget);
}

bool sigroam_scene_probe_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        SrHandshakeState st;
        bool retried;

        /* The tick handler is already inside app->mtx (sigroam.c takes the lock with zero wait
         * before dispatching). Do not take the lock again. */
        if(app->io) {
            SrIoStats stats;

            sr_io_get_stats(app->io, &stats);
            app->hs.rx_bytes_now = stats.rx_bytes;
        }
        app->hs.fw_rev_now = app->model.firmware_rev;
        app->hs.fw_kind = app->model.firmware.kind;
        st = sr_handshake_eval(&app->hs, furi_get_tick());
        retried = false;
        if(sr_handshake_should_retry(&app->hs, furi_get_tick())) {
            if(!probe_queue_info(app)) {
                /* Consume the retry so a full worker slot cannot spin Tick. */
                app->hs.sends = (uint8_t)SR_HANDSHAKE_MAX_SENDS;
                app->probe_send_busy = true;
            } else {
                app->probe_send_busy = false;
            }
            st = sr_handshake_eval(&app->hs, furi_get_tick());
            retried = true;
        }
        if(st == SrHandshakeOk && !app->probe_stop_sent &&
           sr_dialect_probe_should_clear_show_info(&app->model.firmware)) {
            uint32_t snap = app->model.wifi_stop_rev;

            if(probe_queue_stopscan(app)) {
                app->probe_stop_sent = true;
                app->clear_stop_rev = snap;
            }
        }
        /* Retry keeps Waiting, so state equality would skip "Retrying info". */
        if(retried || st != app->hs_shown || app->model.firmware_rev != app->hs_rev_shown) {
            sigroam_scene_probe_draw(app, st);
        }
        return true;
    }

    /* Back not consumed → scene_manager pops to Start. */
    return false;
}

void sigroam_scene_probe_on_exit(void* context) {
    SigRoamApp* app = context;
    widget_reset(app->widget);
}
