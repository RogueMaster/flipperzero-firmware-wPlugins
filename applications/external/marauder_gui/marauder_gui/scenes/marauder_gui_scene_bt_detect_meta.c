#include "../marauder_gui_app_i.h"

/* One line per detection: "Meta Device: <rssi> <name-or-mac>" (see WiFiScan.cpp's
   bluetoothScanAllCallback matching isMetaIdentifier() for BT_SCAN_RAYBAN) - no list -X flag. */
static void marauder_gui_scene_bt_detect_meta_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;
    marauder_gui_log_scan_append(app, line);
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Meta Dedektoru", "Meta Detector"));
}

static void marauder_gui_scene_bt_detect_meta_start(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "sniffbt -t meta");
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Meta Dedektoru", "Meta Detector"));
}

void marauder_gui_scene_bt_detect_meta_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_bt_detect_meta_uart_line;
    app->resume_handler = marauder_gui_scene_bt_detect_meta_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_bt_detect_meta_start(app);
}

bool marauder_gui_scene_bt_detect_meta_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back(context, event);
}

void marauder_gui_scene_bt_detect_meta_on_exit(void* context) {
    marauder_gui_log_scan_exit(context);
}
