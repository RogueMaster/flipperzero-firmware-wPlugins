#include "../dnie_reader.h"
#include "dnie_reader_scene.h"

// Renombrar una lectura guardada con el teclado del Flipper. Vacio = nombre del documento.
// El teclado se crea al entrar y se libera al salir (no ocupa RAM el resto del tiempo).

static void rename_done(void* context) {
    DnieApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DnieCustomEventRenameDone);
}

void dnie_scene_rename_on_enter(void* context) {
    DnieApp* app = context;
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DnieViewTextInput, text_input_get_view(app->text_input));
    strlcpy(app->text_buf, app->saved_alias, sizeof(app->text_buf));
    text_input_set_header_text(app->text_input, "Name for this reading");
    text_input_set_minimum_length(app->text_input, 0);
    text_input_set_result_callback(
        app->text_input, rename_done, app, app->text_buf, sizeof(app->text_buf), false);
    view_dispatcher_switch_to_view(app->view_dispatcher, DnieViewTextInput);
}

bool dnie_scene_rename_on_event(void* context, SceneManagerEvent event) {
    DnieApp* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == DnieCustomEventRenameDone) {
        if(dnie_store_rename(app->saved_path, app->text_buf) == DnieStoreOk) {
            strlcpy(app->saved_alias, app->text_buf, sizeof(app->saved_alias));
            app->pending_toast = "Renamed";
            notification_message(app->notifications, &sequence_success);
        } else {
            app->pending_toast = "Error saving";
            notification_message(app->notifications, &sequence_error);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void dnie_scene_rename_on_exit(void* context) {
    DnieApp* app = context;
    view_dispatcher_remove_view(app->view_dispatcher, DnieViewTextInput);
    text_input_free(app->text_input);
    app->text_input = NULL;
    memset(app->text_buf, 0, sizeof(app->text_buf));
}
