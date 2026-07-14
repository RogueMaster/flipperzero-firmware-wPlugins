#include "../gemini_app_i.h"
#include "storage/storage.h"

#define TEXT_BUFFER_SIZE 128
static char text_buffer[TEXT_BUFFER_SIZE];
static bool requesting_password = false;
static char ssid_buffer[TEXT_BUFFER_SIZE];

enum {
    GeminiSceneStartConnectNewAPOk,
} GeminiSceneStartConnectNewAP;

static void gemini_scene_connect_new_ap_text_input_callback(void* context) {
    GeminiApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, GeminiSceneStartConnectNewAPOk);
}

void gemini_scene_connect_new_ap_on_enter(void* context) {
    GeminiApp* app = context;
    if(requesting_password) {
        text_input_set_header_text(app->text_input, "Enter Password");
    } else {
        text_input_set_header_text(app->text_input, "Enter SSID");
    }
    text_buffer[0] = '\0';
    text_input_set_result_callback(
        app->text_input,
        gemini_scene_connect_new_ap_text_input_callback,
        app,
        text_buffer,
        TEXT_BUFFER_SIZE,
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewTextInput);
}

bool gemini_scene_connect_new_ap_on_event(void* context, SceneManagerEvent event) {
    GeminiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GeminiSceneStartConnectNewAPOk:
            if(!requesting_password) {
                uart_helper_send(app->uart_helper, text_buffer, 0);

                strncpy(ssid_buffer, text_buffer, TEXT_BUFFER_SIZE);

                requesting_password = true;
                gemini_scene_connect_new_ap_on_enter(app);
            } else {
                uart_helper_send(app->uart_helper, text_buffer, 0);

                const char* ap_path = EXT_PATH("apps_data/gemini_ia/SavedAPs.txt");
                Storage* storage = furi_record_open(RECORD_STORAGE);
                File* file = storage_file_alloc(storage);

                bool append_newline = storage_file_exists(storage, ap_path);

                if(storage_file_open(
                       file, ap_path, FSAM_WRITE, FSOM_OPEN_ALWAYS | FSOM_OPEN_APPEND)) {
                    if(append_newline) {
                        storage_file_write(file, "\n", 1);
                    }
                    storage_file_write(file, ssid_buffer, strlen(ssid_buffer));
                    storage_file_write(file, "//", 2);
                    storage_file_write(file, text_buffer, strlen(text_buffer));
                    storage_file_close(file);
                }

                storage_file_free(file);
                furi_record_close(RECORD_STORAGE);

                gemini_scene_receive_serial_set_next(app, GeminiSceneConnectNewAP);
                scene_manager_search_and_switch_to_another_scene(
                    app->scene_manager, GeminiSceneReceiveSerial);
            }

            consumed = true;
            break;
        }
    }

    return consumed;
}

void gemini_scene_connect_new_ap_on_exit(void* context) {
    UNUSED(context);
}
