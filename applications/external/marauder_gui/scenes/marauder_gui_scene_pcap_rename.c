#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <string.h>

/* Renames the .pcap file currently open in the viewer (app->pcap_view_filename) - reached from
   the "Rename>" row at the bottom of pcap_filter.c. Only the base name is editable: the ".pcap"
   extension is stripped into pcap_rename_buffer before the keyboard opens and reattached before
   the actual rename, so the user can't type a different extension and end up with a file
   pcap_file_list.c won't recognize as a capture anymore. */

enum {
    PcapRenameEventDone,
};

static void marauder_gui_scene_pcap_rename_callback(void* context) {
    MarauderGuiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PcapRenameEventDone);
}

void marauder_gui_scene_pcap_rename_on_enter(void* context) {
    MarauderGuiApp* app = context;

    strncpy(app->pcap_rename_buffer, app->pcap_view_filename, sizeof(app->pcap_rename_buffer) - 1);
    app->pcap_rename_buffer[sizeof(app->pcap_rename_buffer) - 1] = '\0';
    size_t len = strlen(app->pcap_rename_buffer);
    if(len > 5 && strcmp(app->pcap_rename_buffer + len - 5, ".pcap") == 0) {
        app->pcap_rename_buffer[len - 5] = '\0';
    }

    marauder_text_input_set_header_text(
        app->text_input, marauder_gui_text(app, "Yeni Dosya Adi", "New File Name"));
    marauder_text_input_set_result_callback(
        app->text_input,
        marauder_gui_scene_pcap_rename_callback,
        app,
        app->pcap_rename_buffer,
        sizeof(app->pcap_rename_buffer),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewTextInput);
}

bool marauder_gui_scene_pcap_rename_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event == PcapRenameEventDone) {
        /* An empty name (user cleared it) or one containing '/' (the symbol keyboard has a '/'
           key, which would otherwise let a "rename" write outside MARAUDER_GUI_PCAP_DIR) is
           treated as a no-op - just back out without touching the file. */
        if(app->pcap_rename_buffer[0] != '\0' && !strchr(app->pcap_rename_buffer, '/')) {
            char old_path[128];
            char new_name[64];
            char new_path[128];
            snprintf(
                old_path,
                sizeof(old_path),
                "%s/%s",
                MARAUDER_GUI_PCAP_DIR,
                app->pcap_view_filename);
            snprintf(new_name, sizeof(new_name), "%s.pcap", app->pcap_rename_buffer);
            snprintf(new_path, sizeof(new_path), "%s/%s", MARAUDER_GUI_PCAP_DIR, new_name);

            if(storage_common_rename(app->storage, old_path, new_path) == FSE_OK) {
                strncpy(app->pcap_view_filename, new_name, sizeof(app->pcap_view_filename) - 1);
                app->pcap_view_filename[sizeof(app->pcap_view_filename) - 1] = '\0';
                /* Only the name changed on disk, not the bytes - retarget the already-parsed
                   index to the new name instead of forcing pcap_view.c to rebuild it. */
                strncpy(
                    app->pcap_index_loaded_filename,
                    app->pcap_view_filename,
                    sizeof(app->pcap_index_loaded_filename) - 1);
                app->pcap_index_loaded_filename[sizeof(app->pcap_index_loaded_filename) - 1] =
                    '\0';
            }
        }
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_pcap_rename_on_exit(void* context) {
    MarauderGuiApp* app = context;
    marauder_text_input_reset(app->text_input);
}
