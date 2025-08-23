#include "../flipper_share_app.h"


// Callback for handling button presses in the dialog
static void dialog_ex_callback(DialogExResult result, void* context) {
    furi_assert(context);
    FlipperShareApp* app = context;
    
    if(result == DialogExResultLeft || result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void flipper_share_scene_show_file_on_enter(void* context) {
    if(!context) {
        return;
    }

    FlipperShareApp* app = context;

    // Additional safety checks
    if(!app || !app->dialog_show_file || !app->view_dispatcher) {
        return;
    }

    // Use the selected file path from app->selected_file_path
    const char* file_path = app->selected_file_path[0] ? app->selected_file_path : "No file selected";

    // Configure dialog with file information
    dialog_ex_set_header(app->dialog_show_file, "File Selected", 64, 10, AlignCenter, AlignCenter);
    dialog_ex_set_text(app->dialog_show_file, file_path, 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_show_file, "Back");
    dialog_ex_set_right_button_text(app->dialog_show_file, "OK");
    
    // Important: set up the callback for dialog buttons
    dialog_ex_set_context(app->dialog_show_file, app);
    dialog_ex_set_result_callback(app->dialog_show_file, dialog_ex_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperShareViewIdShowFile);
}

bool flipper_share_scene_show_file_on_event(void* context, SceneManagerEvent event) {
    if(!context) {
        return false;
    }

    FlipperShareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DialogExResultLeft) {
            // Back button pressed - return to file browser
            if(app->scene_manager) {
                scene_manager_previous_scene(app->scene_manager);
            }
            consumed = true;
        } else if(event.event == DialogExResultRight) {
            // OK button pressed - start reading file
            if(app->scene_manager) {
                scene_manager_next_scene(app->scene_manager, FlipperShareSceneSend);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Handle back button (just in case)
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void flipper_share_scene_show_file_on_exit(void* context) {
    UNUSED(context);
}
