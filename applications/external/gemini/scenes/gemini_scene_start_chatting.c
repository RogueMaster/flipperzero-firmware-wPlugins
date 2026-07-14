#include "../gemini_app_i.h"

#define TEXT_BUFFER_SIZE 128
static char text_buffer[TEXT_BUFFER_SIZE];

enum {
    GeminiSceneStartChattingEventOk,
} GeminiSceneStartChattingEvent;

static void gemini_scene_start_chatting_text_input_callback(void* context) {
    GeminiApp* app = context;
    scene_manager_handle_custom_event(app->scene_manager, GeminiSceneStartChattingEventOk);
}

void gemini_scene_start_chatting_on_enter(void* context) {
    GeminiApp* app = context;
    text_input_set_header_text(app->text_input, "Enter message");
    text_buffer[0] = '\0';
    text_input_set_result_callback(
        app->text_input,
        gemini_scene_start_chatting_text_input_callback,
        app,
        text_buffer,
        TEXT_BUFFER_SIZE,
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, GeminiViewTextInput);
}

bool gemini_scene_start_chatting_on_event(void* context, SceneManagerEvent event) {
    GeminiApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GeminiSceneStartChattingEventOk:
            if(strlen(text_buffer) > 0) {
                uart_helper_send(app->uart_helper, text_buffer, 0);
                // We want BACK to go back to the start chatting scene.
                gemini_scene_receive_serial_set_next(app, GeminiSceneStartChatting);
                scene_manager_search_and_switch_to_another_scene(
                    app->scene_manager, GeminiSceneReceiveSerial);
                consumed = true;
            } else {
                scene_manager_previous_scene(app->scene_manager);
                consumed = true;
            }
            break;
        }
    }

    return consumed;
}

void gemini_scene_start_chatting_on_exit(void* context) {
    UNUSED(context);
}
