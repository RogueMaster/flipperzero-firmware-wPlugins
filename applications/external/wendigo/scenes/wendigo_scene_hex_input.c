#include "../wendigo_app_i.h"

void wendigo_scene_hex_input_callback(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_hex_input_callback()");
    WendigoApp* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, Wendigo_EventStartConsole);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_hex_input_callback()");
}

void wendigo_scene_hex_input_on_enter(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_hex_input_on_enter()");
    WendigoApp* app = context;
    app->current_view = WendigoAppViewHexInput;

    // Setup view
    Wendigo_TextInput* text_input = app->hex_input;
    // Add help message to header
    wendigo_hex_input_set_header_text(text_input, "Send HEX packet to UART");
    wendigo_hex_input_set_result_callback(
        text_input,
        wendigo_scene_hex_input_callback,
        app,
        app->text_input_store,
        WENDIGO_TEXT_INPUT_STORE_SIZE,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, WendigoAppViewHexInput);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_hex_input_on_enter()");
}

bool wendigo_scene_hex_input_on_event(void* context, SceneManagerEvent event) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_hex_input_on_event()");
    WendigoApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == Wendigo_EventStartConsole) {
            // Point to custom string to send
            app->selected_tx_string = app->text_input_store;
            scene_manager_next_scene(app->scene_manager, WendigoSceneConsoleOutput);
            consumed = true;
        }
    }
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_hex_input_on_event()");
    return consumed;
}

void wendigo_scene_hex_input_on_exit(void* context) {
    FURI_LOG_T(WENDIGO_TAG, "Start wendigo_scene_hex_input_on_exit()");
    WendigoApp* app = context;

    wendigo_hex_input_reset(app->hex_input);
    FURI_LOG_T(WENDIGO_TAG, "End wendigo_scene_hex_input_on_exit()");
}
