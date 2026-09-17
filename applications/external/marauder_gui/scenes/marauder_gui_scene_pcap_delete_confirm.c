#include "../marauder_gui_app_i.h"
#include <stdio.h>

/* Confirms and performs deleting the .pcap file currently open in the viewer - reached from the
   "Delete File>" row at the bottom of pcap_filter.c. Destructive and irreversible, so it's its
   own screen (Back cancels, only the Center button confirms) rather than firing straight off the
   menu row like Rename does. */

enum {
    PcapDeleteConfirmEventOk,
};

static void marauder_gui_scene_pcap_delete_confirm_button_callback(
    GuiButtonType button_type,
    InputType input_type,
    void* context) {
    MarauderGuiApp* app = context;
    if(button_type != GuiButtonTypeCenter || input_type != InputTypeShort) return;
    view_dispatcher_send_custom_event(app->view_dispatcher, PcapDeleteConfirmEventOk);
}

void marauder_gui_scene_pcap_delete_confirm_on_enter(void* context) {
    MarauderGuiApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        2,
        AlignCenter,
        AlignTop,
        FontPrimary,
        marauder_gui_text(app, "Dosyayi Sil", "Delete File"));
    widget_add_text_box_element(
        app->widget, 0, 14, 128, 22, AlignCenter, AlignTop, app->pcap_view_filename, false);
    widget_add_string_element(
        app->widget,
        64,
        45,
        AlignCenter,
        AlignTop,
        FontSecondary,
        marauder_gui_text(app, "Bu islem geri alinamaz", "This cannot be undone"));
    widget_add_button_element(
        app->widget,
        GuiButtonTypeCenter,
        marauder_gui_text(app, "Sil", "Delete"),
        marauder_gui_scene_pcap_delete_confirm_button_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewWidget);
}

bool marauder_gui_scene_pcap_delete_confirm_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == PcapDeleteConfirmEventOk) {
        char path[128];
        snprintf(path, sizeof(path), "%s/%s", MARAUDER_GUI_PCAP_DIR, app->pcap_view_filename);
        storage_common_remove(app->storage, path);

        /* The file is gone - drop the cached index so nothing tries to read it again, then back
           out past pcap_filter and pcap_view (there's nothing left there to show) straight to
           the file list, whose next on_enter re-scans the directory and simply won't list it. */
        if(app->pcap_index) {
            free(app->pcap_index);
            app->pcap_index = NULL;
        }
        app->pcap_index_count = 0;
        app->pcap_index_capacity = 0;
        app->pcap_index_loaded_filename[0] = '\0';

        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, MarauderGuiScenePcapFileList);
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_delete_confirm_on_exit(void* context) {
    MarauderGuiApp* app = context;
    widget_reset(app->widget);
}
