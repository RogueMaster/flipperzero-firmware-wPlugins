#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Runs a Marauder sniff/wardrive command with "-serial" so the ESP32 streams raw PCAP back over
   the wire, and writes those bytes straight into a sequential .pcap file on the Flipper SD card.
   The actual byte draining/writing/live-classifying happens in the shared app tick
   (marauder_gui.c) whenever app->is_writing_pcap is set; this scene opens/closes the file and
   shows the results as a live per-AP dashboard: one row per AP, with a running count of how many
   packets of each interesting type have been seen from it (e.g. "MyWiFi | 115 data | 5 auth | 90
   probe |") - not a raw packet-by-packet feed, so the user can tell at a glance whether the
   traffic they're after (a handshake, a specific AP's data, ...) is showing up without having to
   read anything. Reuses ap_list/MarauderGuiViewWifiList like every other "list of rows" scene;
   rows are reformatted from app->pcap_live_aps (see marauder_gui_pcap_live_frame_callback in
   marauder_gui.c) whenever pcap_live_dirty says something changed, throttled to a few times a
   second so this doesn't reformat MARAUDER_AP_LIST_MAX strings on every single 100ms tick.

   For the "targeted" PMKID modes (app->pcap_ap_scoped) the flow scanned and selected an AP first;
   here we "select -a" it so Marauder's "-l" (run on selected list) has a target, then toggle that
   selection back off on exit.

   We also append "-c <channel of that AP>". sniffpmkid tracks a single global channel
   (wifi_scan_obj.set_channel) and "-l" only filters WHICH APs are targeted - it does not follow
   their channel. Without "-c" the sniff runs on whatever channel some earlier command happened to
   leave set (which is also exactly what the "Starting TARGETED PMKID sniff ... on channel N"
   line reports), so a target on ch 8 would be sniffed on, say, ch 132 and capture nothing. */

#define PCAP_LIVE_REFRESH_TICKS 5 /* ~0.5s at the app's 100ms tick period */
#define PCAP_LIVE_MARQUEE_TICKS 3 /* advance the marquee once every N ticks - "slowly" */
#define PCAP_LIVE_MARQUEE_DELAY_TICKS \
    30 /* ~3s pause on a newly-highlighted row before it scrolls */

/* AP lines look like "[N][CH:6] ssid rssi" - same parser wifi_attack.c uses. */
static int marauder_gui_scene_pcap_sniff_parse_channel(const char* line) {
    const char* p = strstr(line, "CH:");
    if(!p) return -1;
    return (int)strtol(p + 3, NULL, 10);
}

/* Row text for one tracked AP - BSSID text as a placeholder name until a Beacon/ProbeResp for it
   supplies the real SSID (see marauder_gui_pcap_live_frame_callback). Long rows are fine: the
   selected row marquee-scrolls (same mechanism wifi_scanning.c's AP list already uses), so
   nothing here needs to fit 128px on its own. */
static void marauder_gui_scene_pcap_sniff_format_row(MarauderGuiApp* app, size_t i) {
    const MarauderPcapLiveApStat* ap = &app->pcap_live_aps[i];
    char name[18];
    if(ap->ssid[0]) {
        strncpy(name, ap->ssid, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    } else {
        snprintf(
            name,
            sizeof(name),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            ap->bssid[0],
            ap->bssid[1],
            ap->bssid[2],
            ap->bssid[3],
            ap->bssid[4],
            ap->bssid[5]);
    }
    snprintf(
        app->ap_list[i],
        MARAUDER_LINE_MAX,
        "%s | %lu data | %lu auth | %lu probe |",
        name,
        (unsigned long)ap->counts[MarauderPcapFrameData],
        (unsigned long)ap->counts[MarauderPcapFrameAuth],
        (unsigned long)ap->counts[MarauderPcapFrameProbe]);
}

static void marauder_gui_scene_pcap_sniff_refresh_rows(MarauderGuiApp* app) {
    app->ap_count = app->pcap_live_ap_count;
    for(size_t i = 0; i < app->pcap_live_ap_count; i++) {
        marauder_gui_scene_pcap_sniff_format_row(app, i);
    }
    app->pcap_live_dirty = false;
    marauder_gui_wifi_list_redraw(app);
}

static void marauder_gui_scene_pcap_sniff_tick(MarauderGuiApp* app) {
    app->pcap_live_refresh_tick++;
    if(app->pcap_live_refresh_tick >= PCAP_LIVE_REFRESH_TICKS) {
        app->pcap_live_refresh_tick = 0;
        if(app->pcap_live_dirty) {
            marauder_gui_scene_pcap_sniff_refresh_rows(app);
        }
    }

    /* elements_scrollable_text_line derives scroll speed straight from how fast this counter
       grows, not from how often we redraw - see the identical pattern in wifi_scanning.c. */
    if(app->wifi_list_marquee_delay > 0) {
        app->wifi_list_marquee_delay--;
    } else {
        app->wifi_list_marquee_hold++;
        if(app->wifi_list_marquee_hold >= PCAP_LIVE_MARQUEE_TICKS) {
            app->wifi_list_marquee_hold = 0;
            app->wifi_list_marquee_tick++;
            marauder_gui_wifi_list_redraw(app);
        }
    }
}

void marauder_gui_scene_pcap_sniff_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->pcap_bytes = 0;
    app->is_writing_pcap = false;
    app->pcap_save_name[0] = '\0';

    /* Start from a clean demux state (and drop any stale buffered bytes from a previous scene). */
    marauder_uart_reset_capture(app->uart);
    marauder_pcap_live_parser_reset(&app->pcap_live_parser);
    app->pcap_live_ap_count = 0;
    app->pcap_live_dirty = false;
    app->pcap_live_refresh_tick = 0;

    /* Make sure the target folders exist (mkdir is a no-op if they already do). */
    storage_common_mkdir(app->storage, MARAUDER_GUI_DATA_DIR);
    storage_common_mkdir(app->storage, MARAUDER_GUI_PCAP_DIR);

    char* path = sequential_file_resolve_path(
        app->storage, MARAUDER_GUI_PCAP_DIR, app->pcap_sniff_prefix, "pcap");
    if(path != NULL) {
        const char* slash = strrchr(path, '/');
        const char* base = slash ? slash + 1 : path;
        strncpy(app->pcap_save_name, base, sizeof(app->pcap_save_name) - 1);
        app->pcap_save_name[sizeof(app->pcap_save_name) - 1] = '\0';

        if(storage_file_open(app->capture_file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            app->is_writing_pcap = true;
        }
        free(path);
    }

    if(!app->is_writing_pcap) {
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget,
            64,
            18,
            AlignCenter,
            AlignTop,
            FontSecondary,
            marauder_gui_text(app, "SD dosyasi acilamadi", "Cannot open SD file"));
        view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
        return;
    }

    /* Targeted ("-l") capture: select the picked AP so Marauder's selected-AP list has a target.
       Must read app->ap_list (still holding the AP-scan results from the previous scene) before
       the WifiList reset below repurposes it for this scene's own live dashboard rows. */
    if(app->pcap_ap_scoped && app->selected_ap_index >= 0 &&
       (size_t)app->selected_ap_index < app->ap_count) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
        marauder_uart_send_line(app->uart, cmd);

        /* Pin the sniff to the target's channel (see the note at the top of this file). */
        int channel =
            marauder_gui_scene_pcap_sniff_parse_channel(app->ap_list[app->selected_ap_index]);
        if(channel > 0) {
            char built[sizeof(app->pcap_cmd_buf)];
            snprintf(built, sizeof(built), "%s -c %d", app->pcap_sniff_cmd, channel);
            snprintf(app->pcap_cmd_buf, sizeof(app->pcap_cmd_buf), "%s", built);
            app->pcap_sniff_cmd = app->pcap_cmd_buf;
        }
    }

    app->ap_count = 0;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = PCAP_LIVE_MARQUEE_DELAY_TICKS;
    app->wifi_scan_frozen = false;
    app->wifi_list_show_selected_count = false;
    /* Points straight at pcap_save_name (a stable app-struct buffer, never freed) rather than a
       copy - showing the file this dashboard's counts are being saved into. */
    app->wifi_list_scanning_label = app->pcap_save_name;
    app->wifi_list_empty_label = marauder_gui_text(app, "AP bekleniyor...", "Waiting for APs...");

    app->tick_handler = marauder_gui_scene_pcap_sniff_tick;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);
    marauder_gui_wifi_list_redraw(app);

    /* "<cmd> -serial\n" - the -serial flag is what makes Marauder stream PCAP over the wire. */
    marauder_uart_send(app->uart, app->pcap_sniff_cmd);
    marauder_uart_send_line(app->uart, " -serial");
}

bool marauder_gui_scene_pcap_sniff_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void marauder_gui_scene_pcap_sniff_on_exit(void* context) {
    MarauderGuiApp* app = context;

    marauder_uart_send_line(app->uart, "stopscan");

    /* Undo the AP selection so it doesn't leak into the next flow ("select" toggles), matching
       wifi_attack.c's cleanup.

       pcap_ap_scoped is deliberately NOT cleared here: Back from this scene returns to the AP
       scan, not to the PMKID menu, so the user can pick another AP and come straight back in.
       Clearing it would skip the "select -a" above on that second run and Marauder would answer
       "You don't have any targets selected". Every path into this scene sets the flag itself
       (the broadcast rows and the channel picker set it false), so it can't leak.

       Only checking ">= 0" (not also "< ap_count" like on_enter does) is deliberate: on_enter
       resets ap_list/ap_count for this scene's own live dashboard rows right after reading the
       target AP out of the original scan data, so by the time on_exit runs, ap_count no longer
       means "how many APs the scan found" - it means "how many APs the dashboard has tracked so
       far". selected_ap_index was already bounds-checked against the real scan count back when
       wifi_scanning.c set it, so that's not lost, just no longer re-checkable here. */
    if(app->pcap_ap_scoped && app->selected_ap_index >= 0) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "select -a %d", app->selected_ap_index);
        marauder_uart_send_line(app->uart, cmd);
    }

    /* Flush any last pcap bytes still queued, then close the file. */
    app->is_writing_pcap = false;
    if(storage_file_is_open(app->capture_file)) {
        uint8_t buf[128];
        size_t n;
        while((n = marauder_uart_receive_pcap(app->uart, buf, sizeof(buf))) > 0) {
            storage_file_write(app->capture_file, buf, n);
        }
        storage_file_close(app->capture_file);
    }

    /* Clear the demux so a missed [BUF/CLOSE] can't leave it stuck in pcap mode and swallow the
       next scene's text output. */
    marauder_uart_reset_capture(app->uart);

    app->uart_line_handler = NULL;
    app->tick_handler = NULL;
    widget_reset(app->widget);
}
