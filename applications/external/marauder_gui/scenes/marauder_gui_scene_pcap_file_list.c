#include "../marauder_gui_app_i.h"
#include <string.h>

/* Lists .pcap files already saved under MARAUDER_GUI_PCAP_DIR (see pcap_sniff.c) so one can be
   opened in the filterable table viewer (pcap_view.c). Purely local (no UART involved) - reuses
   the shared WifiList view/ap_list like every other "pick from a list" scene, just populated
   from a directory read instead of a live scan. */

static bool marauder_gui_scene_pcap_file_list_has_pcap_ext(const char* name) {
    size_t len = strlen(name);
    return len > 5 && strcmp(name + len - 5, ".pcap") == 0;
}

void marauder_gui_scene_pcap_file_list_on_enter(void* context) {
    MarauderGuiApp* app = context;

    app->ap_count = 0;
    app->wifi_list_selected = 0;
    app->wifi_list_scroll_offset = 0;
    app->wifi_list_marquee_tick = 0;
    app->wifi_list_marquee_hold = 0;
    app->wifi_list_marquee_delay = 0;
    app->wifi_scan_frozen =
        true; /* no live scan here - suppress the WifiList "(Back:Stop)" hint */
    app->wifi_list_show_selected_count = false;
    app->wifi_list_frozen_label = marauder_gui_text(app, "Kayitli PCAP'ler", "Saved PCAPs");
    app->wifi_list_empty_label =
        marauder_gui_text(app, "Hic .pcap dosyasi yok", "No .pcap files yet");

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, MARAUDER_GUI_PCAP_DIR)) {
        FileInfo info;
        char name[64];
        while(app->ap_count < MARAUDER_AP_LIST_MAX &&
              storage_dir_read(dir, &info, name, sizeof(name))) {
            if(!(info.flags & FSF_DIRECTORY) &&
               marauder_gui_scene_pcap_file_list_has_pcap_ext(name)) {
                strncpy(app->ap_list[app->ap_count], name, MARAUDER_LINE_MAX - 1);
                app->ap_list[app->ap_count][MARAUDER_LINE_MAX - 1] = '\0';
                app->ap_count++;
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWifiList);
    marauder_gui_wifi_list_redraw(app);
}

bool marauder_gui_scene_pcap_file_list_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < app->ap_count) {
            strncpy(
                app->pcap_view_filename,
                app->ap_list[event.event],
                sizeof(app->pcap_view_filename) - 1);
            app->pcap_view_filename[sizeof(app->pcap_view_filename) - 1] = '\0';
            scene_manager_next_scene(app->scene_manager, MarauderGuiScenePcapView);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_file_list_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->wifi_list_frozen_label = NULL;
    app->tick_handler = NULL;
}
