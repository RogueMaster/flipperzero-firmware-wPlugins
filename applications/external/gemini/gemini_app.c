#include "gemini_app_i.h"
#include "storage/storage.h"

#define DEVICE_BAUDRATE        115200
#define LINE_DELIMITER         '\n'
#define INCLUDE_LINE_DELIMITER false

static bool gemini_app_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    GeminiApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

static bool gemini_app_back_event_callback(void* context) {
    furi_assert(context);
    GeminiApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

bool gemini_app_send_api_key(GeminiApp* app) {
    UNUSED(app);
    const char* folder_path = EXT_PATH("apps_data/gemini_ia");
    const char* key_path = EXT_PATH("apps_data/gemini_ia/key.txt");
    bool sent = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_file_exists(storage, key_path)) {
        char key[128];
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, key_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            memset(key, 0, COUNT_OF(key));
            size_t bytes_read = storage_file_read(file, key, COUNT_OF(key));
            if(bytes_read > 0) {
                size_t bytes_send = bytes_read;
                for(size_t i = 0; i < bytes_read; i++) {
                    if((key[i] == '\r') || (key[i] == '\n') || (key[i] == ' ')) {
                        key[i] = '\0';
                        bytes_send = 0;
                        break;
                    }
                }
                uart_helper_send(app->uart_helper, key, bytes_send);
                sent = true;
            }
            storage_file_close(file);
        }
    } else if(!storage_dir_exists(storage, folder_path)) {
        storage_simply_mkdir(storage, folder_path);
    }
    furi_record_close(RECORD_STORAGE);

    return sent;
}

static GeminiApp* gemini_app_alloc() {
    GeminiApp* app = malloc(sizeof(GeminiApp));
    app->scene_manager = scene_manager_alloc(&gemini_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, gemini_app_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, gemini_app_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GeminiViewSubmenu, submenu_get_view(app->submenu));
    app->gemini_text_box = gemini_text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        GeminiViewGeminiTextBox,
        gemini_text_box_get_view(app->gemini_text_box));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GeminiViewTextInput, text_input_get_view(app->text_input));
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, GeminiViewWidget, widget_get_view(app->widget));

    app->uart_helper = uart_helper_alloc();
    uart_helper_set_baud_rate(app->uart_helper, DEVICE_BAUDRATE);
    uart_helper_set_delimiter(app->uart_helper, 254, INCLUDE_LINE_DELIMITER);
    uart_helper_set_callback(app->uart_helper, NULL, NULL);

    return app;
}

static void gemini_app_free(GeminiApp* app) {
    furi_assert(app);
    uart_helper_free(app->uart_helper);
    view_dispatcher_remove_view(app->view_dispatcher, GeminiViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, GeminiViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, GeminiViewGeminiTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, GeminiViewWidget);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    gemini_text_box_free(app->gemini_text_box);
    text_input_free(app->text_input);
    widget_free(app->widget);
    free(app);
}

int32_t gemini_app(void* p) {
    UNUSED(p);
    GeminiApp* app = gemini_app_alloc();

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    if(gemini_app_send_api_key(app)) {
        scene_manager_next_scene(app->scene_manager, GeminiSceneMainMenu);
    } else {
        scene_manager_next_scene(app->scene_manager, GeminiSceneMissingApiKey);
    }
    view_dispatcher_run(app->view_dispatcher);

    gemini_app_free(app);
    return 0;
}
