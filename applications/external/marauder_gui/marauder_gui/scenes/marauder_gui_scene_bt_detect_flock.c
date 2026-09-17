#include "../marauder_gui_app_i.h"

/* Flock detection is dual-radio (WiFi probe/beacon bait + a real BLE scan running at the same
   time, see WiFiScan.cpp's StartScan() for BT_SCAN_FLOCK) and Marauder itself only keeps a raw
   counter for it upstream - no list, no dedicated struct (explicit "To-do" in the source). The
   WiFi-side lines also embed raw TFT color-control bytes meant for the device's own screen
   parser; we strip anything non-printable before logging so they don't render as garbage. */
static void marauder_gui_scene_bt_detect_flock_uart_line(MarauderGuiApp* app, const char* line) {
    if(line[0] == '\0') return;

    char clean[MARAUDER_LINE_MAX];
    size_t i = 0;
    for(; line[i] != '\0' && i < sizeof(clean) - 1; i++) {
        unsigned char c = (unsigned char)line[i];
        clean[i] = (c < 0x20) ? ' ' : (char)c;
    }
    clean[i] = '\0';

    marauder_gui_log_scan_append(app, clean);
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Flock Dedektoru", "Flock Detector"));
}

static void marauder_gui_scene_bt_detect_flock_start(MarauderGuiApp* app) {
    marauder_uart_send_line(app->uart, "sniffbt -t flock");
    marauder_gui_log_scan_redraw(app, marauder_gui_text(app, "Flock Dedektoru", "Flock Detector"));
}

void marauder_gui_scene_bt_detect_flock_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_log_scan_clear(app);
    app->wifi_scan_frozen = false;
    app->uart_line_handler = marauder_gui_scene_bt_detect_flock_uart_line;
    app->resume_handler = marauder_gui_scene_bt_detect_flock_start;

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);

    marauder_gui_scene_bt_detect_flock_start(app);
}

bool marauder_gui_scene_bt_detect_flock_on_event(void* context, SceneManagerEvent event) {
    return marauder_gui_log_scan_handle_back(context, event);
}

void marauder_gui_scene_bt_detect_flock_on_exit(void* context) {
    marauder_gui_log_scan_exit(context);
}
