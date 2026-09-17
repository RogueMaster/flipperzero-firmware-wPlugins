#include "../sigroam.h"

#include <stdio.h>
#include <string.h>

static const char* field_or_q(const char* s) {
    return (s != NULL && s[0] != '\0') ? s : "?";
}

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
                "Need a Marauder\n"
                "board on GPIO\n"
                "13/14, 5V on pin 1.\n"
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
        snprintf(
            app->probe_text,
            sizeof(app->probe_text),
            "\e#Probing...\n"
            "\n"
            "Sent: info\n"
            "Waiting up to 1.5s");
        break;
    case SrHandshakeOk:
        snprintf(
            app->probe_text,
            sizeof(app->probe_text),
            "\e#%s\n"
            "Version: %s\n"
            "Hardware: %s\n"
            "ESP-IDF: %s\n",
            field_or_q(fw->firmware),
            field_or_q(fw->version),
            field_or_q(fw->hardware),
            field_or_q(fw->esp_idf));
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
        snprintf(
            app->probe_text,
            sizeof(app->probe_text),
            "\e#No reply\n"
            "\n"
            "Nothing in 1.5s.\n"
            "\n"
            "Board may still be\n"
            "booting. Wait a few\n"
            "seconds and retry.\n"
            "\n"
            "Else check pin 13/14\n"
            "wiring, board power,\n"
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
    widget_add_text_scroll_element(
        app->widget, 0, 0, SR_CANVAS_W, SR_CANVAS_H, app->probe_text);
    app->hs_shown = st;
    app->hs_rev_shown = app->model.firmware_rev;
}

void sigroam_scene_probe_on_enter(void* context) {
    SigRoamApp* app = context;
    SrHandshakeState st;

    memset(&app->hs, 0, sizeof(app->hs));
    app->hs.timeout_ms = SR_HANDSHAKE_TIMEOUT_MS;
    app->probe_send_busy = false;

    if(app->io && sr_io_is_open(app->io) && app->worker) {
        SrIoStats stats;

        sr_io_get_stats(app->io, &stats);
        app->hs.rx_bytes_at_send = stats.rx_bytes;
        app->hs.rx_bytes_now = stats.rx_bytes;
        app->hs.fw_rev_at_send = app->model.firmware_rev;
        app->hs.fw_rev_now = app->model.firmware_rev;
        app->hs.sent_tick_ms = furi_get_tick();
        app->hs.sent = sr_worker_send_cmd(app->worker, "info\n");
        app->probe_send_busy = !app->hs.sent;
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
        if(st != app->hs_shown || app->model.firmware_rev != app->hs_rev_shown) {
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
