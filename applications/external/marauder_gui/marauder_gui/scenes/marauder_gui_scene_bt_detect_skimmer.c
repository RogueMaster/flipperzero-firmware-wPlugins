#include "../marauder_gui_app_i.h"
#include <string.h>

/* Marauder's own skimmer match check only ever reaches its TFT, never Serial - the serial
   stream is an UNFILTERED firehose of every BLE ad seen, printed as "<name><rssi>" with no
   separator (e.g. "HC-05-62") - see WiFiScan.cpp's bluetoothScanAllCallback for BT_SCAN_SKIMMERS.
   So we replicate the name match client-side against the same bad_list Marauder itself uses. */
static bool marauder_gui_scene_bt_detect_skimmer_is_match(const char* line) {
    return strncmp(line, "HC-03", 5) == 0 || strncmp(line, "HC-05", 5) == 0 ||
           strncmp(line, "HC-06", 5) == 0;
}

static void marauder_gui_scene_bt_detect_skimmer_uart_line(MarauderGuiApp* app, const char* line) {
    if(!marauder_gui_scene_bt_detect_skimmer_is_match(line)) return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Skimmer Tespiti", "Skimmer Detection"));
}

static void marauder_gui_scene_bt_detect_skimmer_start(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "sniffskim");
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Skimmer Tespiti", "Skimmer Detection"));
}

void marauder_gui_scene_bt_detect_skimmer_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_bt_detect_skimmer_uart_line;
    app->resume_handler = marauder_gui_scene_bt_detect_skimmer_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_bt_detect_skimmer_start(app);
}

bool marauder_gui_scene_bt_detect_skimmer_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back(context, event);
}

void marauder_gui_scene_bt_detect_skimmer_on_exit(void* context) {
    marauder_gui_log_scan_exit(context);
}
