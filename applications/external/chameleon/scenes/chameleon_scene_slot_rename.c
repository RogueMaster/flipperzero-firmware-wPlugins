#include "../chameleon_app_i.h"

static void chameleon_scene_slot_rename_text_input_callback(void* context) {
    ChameleonApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void chameleon_scene_slot_rename_on_enter(void* context) {
    ChameleonApp* app = context;
    TextInput* text_input = app->text_input;

    // Copy current nickname to buffer
    strncpy(app->text_buffer, app->slots[app->active_slot].nickname, sizeof(app->text_buffer) - 1);

    text_input_reset(text_input);
    text_input_set_header_text(text_input, "Enter slot name:");
    text_input_set_result_callback(
        text_input,
        chameleon_scene_slot_rename_text_input_callback,
        app,
        app->text_buffer,
        32,
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewTextInput);
}

bool chameleon_scene_slot_rename_on_event(void* context, SceneManagerEvent event) {
    ChameleonApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Save nickname
        if(chameleon_app_set_slot_nickname(app, app->active_slot, app->text_buffer)) {
            popup_set_header(app->popup, "Success", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Slot renamed", 64, 32, AlignCenter, AlignCenter);
        } else {
            popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
            popup_set_text(app->popup, "Failed to rename", 64, 32, AlignCenter, AlignCenter);
        }

        view_dispatcher_switch_to_view(app->view_dispatcher, ChameleonViewPopup);
        furi_delay_ms(1500);

        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void chameleon_scene_slot_rename_on_exit(void* context) {
    ChameleonApp* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
