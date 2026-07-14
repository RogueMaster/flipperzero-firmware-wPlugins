#include "../gemini_app_i.h"
#include "storage/storage.h"

static bool gemini_app_send_access_points(GeminiApp* app) {
    const char* ap_path = EXT_PATH("apps_data/gemini_ia/SavedAPs.txt");
    bool sent = false;

    FuriString* access_points = furi_string_alloc();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_file_exists(storage, ap_path)) {
        char ap[128];
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, ap_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            size_t seek_offset = 0;
            while(true) {
                memset(ap, 0, COUNT_OF(ap));
                size_t bytes_read = storage_file_read(file, ap, COUNT_OF(ap));
                if(bytes_read > 0) {
                    size_t ap_len = bytes_read + 1; // Add one for the null character.
                    for(size_t i = 0; i < bytes_read; i++) {
                        if((ap[i] == '\r') || (ap[i] == '\n')) {
                            ap[i] = '\0';
                            ap_len = i;
                            seek_offset += (i + 1);
                            storage_file_seek(file, seek_offset, true);
                            break;
                        }
                    }
                    if(ap_len > 0) {
                        if(furi_string_size(access_points) > 0) {
                            furi_string_cat(access_points, ", ");
                        }
                        furi_string_cat(access_points, ap);
                    }
                } else {
                    break;
                }
            }
            storage_file_close(file);
        }
    }
    furi_record_close(RECORD_STORAGE);

    if(furi_string_size(access_points) > 0) {
        furi_string_cat(access_points, "\n");
        uart_helper_send(app->uart_helper, furi_string_get_cstr(access_points), 0);
        sent = true;
    } else {
        uart_helper_send(app->uart_helper, "", 0);
    }

    furi_string_free(access_points);

    return sent;
}

void gemini_scene_send_known_aps_on_enter(void* context) {
    GeminiApp* app = context;
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 0, 25, AlignLeft, AlignTop, FontPrimary, "Please wait...");
    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewWidget);

    // Wait for the scan of APs to happen. (TODO: Implement a better way to wait for the scan to finish)
    furi_delay_ms(5000);
    widget_reset(app->widget);

    if(gemini_app_send_access_points(app)) {
        widget_add_string_element(
            app->widget, 0, 25, AlignLeft, AlignTop, FontPrimary, "SENT APs");
        view_dispatcher_send_custom_event(app->view_dispatcher, 42);
    } else {
        widget_add_string_element(
            app->widget, 0, 25, AlignLeft, AlignTop, FontPrimary, "Required manual connect");
    }
}

bool gemini_scene_send_known_aps_on_event(void* context, SceneManagerEvent event) {
    GeminiApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 42) {
            gemini_scene_receive_serial_set_next(app, GeminiSceneMainMenu);
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, GeminiSceneReceiveSerial);
            return true;
        }
    }

    return false;
}

void gemini_scene_send_known_aps_on_exit(void* context) {
    UNUSED(context);
}
