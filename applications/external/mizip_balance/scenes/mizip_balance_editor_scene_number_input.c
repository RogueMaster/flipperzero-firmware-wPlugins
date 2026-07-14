#include "../mizip_balance_editor_i.h"

void mizip_balance_editor_scene_input_number_callback(void* context, int32_t number) {
    MiZipBalanceEditorApp* app = context;
    app->new_balance = number;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void mizip_balance_editor_scene_number_input_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    NumberInput* number_input = app->number_input;

    char str[50];
    snprintf(
        str,
        sizeof(str),
        "Balance in cents (%d - %d)",
        MIZIP_BALANCE_MIN_VALUE,
        MIZIP_BALANCE_MAX_VALUE);

    number_input_set_header_text(number_input, str);
    number_input_set_result_callback(
        number_input,
        mizip_balance_editor_scene_input_number_callback,
        context,
        app->current_balance,
        MIZIP_BALANCE_MIN_VALUE,
        MIZIP_BALANCE_MAX_VALUE);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdNumberInput);
}

bool mizip_balance_editor_scene_number_input_on_event(void* context, SceneManagerEvent event) {
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) { //Back button pressed
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return consumed;
}

void mizip_balance_editor_scene_number_input_on_exit(void* context) {
    UNUSED(context);
}
