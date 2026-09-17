#include "marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* Shared plumbing for detectors that stream raw event lines with no "list -X" way to re-query
   them on demand (deauth sniff, pwnagotchi, meta/flock/skimmer BLE detectors, mactrack's
   periodic table) - see marauder_gui_app_i.h for why this differs from marauder_gui_list_scan.c. */

void marauder_gui_log_scan_clear(MarauderGuiApp* app) {
    app->terminal_log[0] = '\0';
    app->terminal_log_len = 0;
}

void marauder_gui_log_scan_append(MarauderGuiApp* app, const char* line) {
    size_t remaining = sizeof(app->terminal_log) - app->terminal_log_len - 1;
    if(remaining < 2) return;

    size_t line_len = strlen(line);
    size_t to_copy = (line_len > remaining - 1) ? remaining - 1 : line_len;

    memcpy(app->terminal_log + app->terminal_log_len, line, to_copy);
    app->terminal_log_len += to_copy;
    app->terminal_log[app->terminal_log_len++] = '\n';
    app->terminal_log[app->terminal_log_len] = '\0';
}

/* A plain Widget (no button element) never forwards Ok anywhere - unlike the WifiList/Menu
   views' own input callbacks, Widget only reacts to keys via elements it explicitly manages. So
   the "Ok while frozen = resume" custom event these scenes' on_event listens for (via
   marauder_gui_log_scan_handle_back(_keep_running)) only actually fires because of this button. */
static void marauder_gui_log_scan_resume_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    MarauderGuiApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MARAUDER_RESUME_CUSTOM_EVENT);
    }
}

void marauder_gui_log_scan_redraw(MarauderGuiApp* app, const char* title) {
    app->log_scan_title = title;

    /* Header is just the title - the "(Back:Stop)" hint that used to be appended here overflowed
       the 128px width on the longer titles. The frozen state is already shown by the "Resume"
       button that appears below when stopped. */
    widget_reset(app->widget);
    widget_add_string_element(app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, title);
    /* Show a placeholder until Marauder's first line arrives, so a quiet detector (no traffic
       yet) still shows it's running instead of a blank screen. */
    widget_add_text_scroll_element(
        app->widget,
        0,
        13,
        128,
        40,
        app->terminal_log_len > 0 ? app->terminal_log :
                                    marauder_gui_text(app, "Dinleniyor...", "Listening..."));
    if(app->wifi_scan_frozen) {
        widget_add_button_element(
            app->widget,
            GuiButtonTypeCenter,
            marauder_gui_text(app, "Devam Et", "Resume"),
            marauder_gui_log_scan_resume_button_callback,
            app);
    }
}

bool marauder_gui_log_scan_handle_back(MarauderGuiApp* app, SceneManagerEvent event) {
    /* Ok while frozen: resume - re-issue the original start command rather than forcing the
       user to back all the way out and re-enter just to keep watching. The scene's own
       resume_handler is expected to redraw itself (it knows its own title text), so the header's
       "(Durdu)"/"(Geri:Dur)" flips immediately instead of waiting on the next line to arrive. */
    if(event.type == SceneManagerEventTypeCustom) {
        if(!app->wifi_scan_frozen || !app->resume_handler) return false;
        app->wifi_scan_frozen = false;
        app->resume_handler(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeBack) return false;

    if(!app->wifi_scan_frozen) {
        app->wifi_scan_frozen = true;
        marauder_uart_send_line(app->uart, "stopscan");
        /* No more lines will arrive to trigger the scene's own redraw once stopped, so redraw
           here (using the title cached by the last real redraw) or the header/"Devam Et" button
           would only ever appear by coincidence, whenever one last queued line slips in. */
        marauder_gui_log_scan_redraw(app, app->log_scan_title);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }

    return true;
}

void marauder_gui_log_scan_exit(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "stopscan");
    app->uart_line_handler = NULL;
    app->resume_handler = NULL;
    widget_reset(app->widget);
}

/* Same as above but never sends "stopscan" - for scenes that run their scan while the ESP32 is
   STA-joined to a real network (ping/arp/port scan). StopScan() -> shutdownWiFi() only skips its
   destructive esp_wifi_deinit()/esp_netif_deinit() teardown when Marauder's own wifi_connected
   flag happens to be true at that exact moment; if the STA link has dropped for any reason (weak
   signal, AP-side timeout, ESP32 resource pressure from a long port scan) by the time the user
   backs out, that flag is false and stopscan permanently kills the WiFi driver until a reboot -
   the exact bug already found and fixed for the Join WiFi flow (see wifi_join_status.c). Leaving
   the scan loop running server-side afterward is a harmless no-op cost by comparison; a later
   pingscan/arpscan/portscan command simply overwrites currentScanMode anyway. */
bool marauder_gui_log_scan_handle_back_keep_running(MarauderGuiApp* app, SceneManagerEvent event) {
    /* Ok while frozen: resume - see marauder_gui_log_scan_handle_back's comment above. Since this
       variant never actually sends "stopscan" on freeze either, the underlying scan never really
       stopped - this just re-issues the start command in case it needs one (some of these, like
       Port Scan, aren't resumable in any meaningful sense since the scan naturally finishes on
       its own, but re-sending is harmless either way). */
    if(event.type == SceneManagerEventTypeCustom) {
        if(!app->wifi_scan_frozen || !app->resume_handler) return false;
        app->wifi_scan_frozen = false;
        app->resume_handler(app);
        return true;
    }

    if(event.type != SceneManagerEventTypeBack) return false;

    if(!app->wifi_scan_frozen) {
        app->wifi_scan_frozen = true;
        /* Unlike the stopscan-sending variant, new lines can still arrive here (nothing was
           actually stopped) - but redraw immediately anyway so the "Devam Et" button shows up
           right away instead of waiting on the next one. */
        marauder_gui_log_scan_redraw(app, app->log_scan_title);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }

    return true;
}

void marauder_gui_log_scan_exit_keep_running(MarauderGuiApp* app) {
    app->uart_line_handler = NULL;
    app->resume_handler = NULL;
    widget_reset(app->widget);
}
