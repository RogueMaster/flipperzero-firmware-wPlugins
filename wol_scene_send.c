#include "wol_flipper.h"

static char send_header[WOL_NAME_LEN + 8];

static void wol_send_report(WolApp* app, WolSendStep step) {
    view_dispatcher_send_custom_event(app->view_dispatcher, WOL_EVENT_SEND(step));
}

/**
 * Errors get the diagnosis and the evidence on the same screen. A serial link
 * to a board with one LED gives nothing to reason about otherwise, and every
 * failure so far has been resolved by looking at what the board actually said.
 */
static void wol_send_show_error(WolApp* app, const char* message) {
    snprintf(
        app->status_text,
        sizeof(app->status_text),
        "\e#%s\n\n\e#Board said\n%s",
        message,
        app->raw_reply[0] ? app->raw_reply : "(nothing)");

    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, app->status_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewWidget);
    notification_message(app->notifications, &sequence_error);
}

/** Called from the worker thread while a board command is in flight. */
static void wol_send_progress(void* context, WolEspProgress progress) {
    WolApp* app = context;

    switch(progress) {
    case WolEspProgressWifi:
        wol_send_report(app, WolSendStepWifi);
        break;
    case WolEspProgressSending:
        wol_send_report(app, WolSendStepPacket);
        break;
    }
}

static WolSendStep wol_send_step_for_error(WolEspResult result) {
    switch(result) {
    case WolEspErrWrongFirmware:
        return WolSendStepErrFirmware;
    case WolEspErrPower:
        return WolSendStepErrPower;
    case WolEspErrReboot:
        return WolSendStepErrReboot;
    case WolEspErrWifi:
        return WolSendStepErrWifi;
    case WolEspErrWifiNotFound:
        return WolSendStepErrWifiNotFound;
    case WolEspErrWifiAuth:
        return WolSendStepErrWifiAuth;
    case WolEspErrScan:
        return WolSendStepErrScan;
    case WolEspErrUdp:
    case WolEspErrArgs:
        return WolSendStepErrSend;
    default:
        return WolSendStepErrBoard;
    }
}

/** Lift one reported line, e.g. "+SEND 6/6 ...", out of the board's reply. */
static void wol_send_copy_line(WolApp* app, WolEsp* esp, const char* prefix) {
    const char* start = strstr(wol_esp_last_reply(esp), prefix);
    if(!start) return;

    size_t len = 0;
    while(start[len] && start[len] != '\n') len++;
    if(len >= sizeof(app->worker_info)) len = sizeof(app->worker_info) - 1;

    memcpy(app->worker_info, start, len);
    app->worker_info[len] = '\0';
}

/** Snapshot what the board said, then report the mapped failure. */
static void wol_send_fail(WolApp* app, WolEsp* esp, WolEspResult result) {
    snprintf(app->raw_reply, sizeof(app->raw_reply), "%s", wol_esp_last_reply(esp));
    wol_send_report(app, wol_send_step_for_error(result));
}

static int32_t wol_send_worker(void* context) {
    WolApp* app = context;
    WolEsp* esp = wol_esp_alloc(&app->worker_cancel);
    const WolTarget* target = &app->config.targets[app->target_index];
    WolEspResult result;
    uint8_t version = 0;

    app->worker_info[0] = '\0';
    app->raw_reply[0] = '\0';
    wol_esp_set_progress_callback(esp, wol_send_progress, app);

    do {
        wol_send_report(app, WolSendStepPower);
        wol_app_ensure_power(app);
        if(!wol_esp_open(esp)) {
            wol_send_report(app, WolSendStepErrBoard);
            break;
        }

        wol_send_report(app, WolSendStepSync);
        result = wol_esp_ping(esp, &version);
        if(result != WolEspOk) {
            wol_send_fail(app, esp, result);
            break;
        }
        if(app->worker_cancel) break;

        if(app->wake_op == WolWakeOpPing) {
            snprintf(
                app->worker_info, sizeof(app->worker_info), "Firmware v%u alive", version);
            wol_send_report(app, WolSendStepDone);
            break;
        }

        if(app->wake_op == WolWakeOpScan) {
            wol_send_report(app, WolSendStepWifi);
            result = wol_esp_scan(esp, app->scan_list, WOL_SSID_MAX_SCAN, &app->scan_count);
            if(result == WolEspOk) {
                wol_send_report(app, WolSendStepDone);
            } else {
                wol_send_fail(app, esp, result);
            }
            break;
        }

        if(app->wake_op == WolWakeOpWifiTest) {
            result = wol_esp_join(esp, app->config.ssid, app->config.pass);
        } else {
            result = wol_esp_wake(
                esp,
                app->config.ssid,
                app->config.pass,
                target->mac,
                target->ip,
                target->port);
        }
        if(app->worker_cancel) break;

        if(result == WolEspOk) {
            // the count and the addresses are the only evidence the packet
            // really went somewhere, so put them on the result screen
            if(app->wake_op == WolWakeOpSend) wol_send_copy_line(app, esp, "+SEND ");
            wol_send_report(app, WolSendStepDone);
        } else {
            wol_send_fail(app, esp, result);
        }
    } while(false);

    wol_esp_close(esp);
    wol_esp_free(esp);
    return 0;
}

void wol_scene_send_on_enter(void* context) {
    WolApp* app = context;

    switch(app->wake_op) {
    case WolWakeOpWifiTest:
        wol_strcpy(send_header, sizeof(send_header), "Wi-Fi test");
        break;
    case WolWakeOpPing:
        wol_strcpy(send_header, sizeof(send_header), "Firmware check");
        break;
    case WolWakeOpScan:
        wol_strcpy(send_header, sizeof(send_header), "Scanning");
        break;
    case WolWakeOpSend:
        wol_strcpy(send_header, sizeof(send_header), app->config.targets[app->target_index].name);
        break;
    }

    popup_reset(app->popup);
    popup_set_header(app->popup, send_header, 64, 8, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Powering board...", 64, 32, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, WolViewPopup);

    app->worker_cancel = false;
    app->worker = furi_thread_alloc_ex("WolSendWorker", 2048, wol_send_worker, app);
    furi_thread_start(app->worker);
}

bool wol_scene_send_on_event(void* context, SceneManagerEvent event) {
    WolApp* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event < WOL_EVENT_SEND_BASE) return false;

    uint32_t step = event.event - WOL_EVENT_SEND_BASE;

    switch(step) {
    case WolSendStepPower:
        popup_set_text(app->popup, "Powering board...", 64, 32, AlignCenter, AlignTop);
        break;
    case WolSendStepSync:
        popup_set_text(app->popup, "Talking to board...", 64, 32, AlignCenter, AlignTop);
        break;
    case WolSendStepWifi:
        popup_set_text(app->popup, "Joining Wi-Fi...", 64, 32, AlignCenter, AlignTop);
        break;
    case WolSendStepPacket:
        popup_set_text(app->popup, "Sending magic packet...", 64, 32, AlignCenter, AlignTop);
        break;
    case WolSendStepDone:
        if(app->wake_op == WolWakeOpScan) {
            scene_manager_next_scene(app->scene_manager, WolSceneWifiScan);
            return true;
        }
        if(app->worker_info[0] != '\0') {
            wol_strcpy(app->status_text, sizeof(app->status_text), app->worker_info);
        } else {
            wol_strcpy(
                app->status_text,
                sizeof(app->status_text),
                app->wake_op == WolWakeOpWifiTest ? "Wi-Fi is up" : "Magic packet sent");
        }
        popup_set_text(app->popup, app->status_text, 64, 32, AlignCenter, AlignTop);
        notification_message(app->notifications, &sequence_success);
        break;
    case WolSendStepErrBoard:
        wol_send_show_error(app, "No answer from board");
        break;
    case WolSendStepErrFirmware:
        wol_send_show_error(app, "Wrong ESP firmware");
        break;
    case WolSendStepErrPower:
        wol_send_show_error(app, "Flipper 5V tripped");
        break;
    case WolSendStepErrReboot:
        wol_send_show_error(app, "Board restarted");
        break;
    case WolSendStepErrWifi:
        wol_send_show_error(app, "Wi-Fi join failed");
        break;
    case WolSendStepErrWifiNotFound:
        wol_send_show_error(app, "Network not on the air");
        break;
    case WolSendStepErrWifiAuth:
        wol_send_show_error(app, "AP refused the key");
        break;
    case WolSendStepErrScan:
        wol_send_show_error(app, "Scan failed");
        break;
    case WolSendStepErrSend:
        wol_send_show_error(app, "UDP send failed");
        break;
    default:
        return false;
    }

    return true;
}

void wol_scene_send_on_exit(void* context) {
    WolApp* app = context;

    app->worker_cancel = true;
    if(app->worker) {
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        app->worker = NULL;
    }
    app->wake_op = WolWakeOpSend;
    popup_reset(app->popup);
    widget_reset(app->widget);
}
