#include "../hermes_i.h"
#include <string.h>

static void hermes_scene_keyboard_callback(void* context) {
    HermesApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventConsoleText);
}

void hermes_scene_keyboard_on_enter(void* context) {
    HermesApp* app = context;

    app->text_buffer[0] = '\0';

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Send to target");
    text_input_set_result_callback(
        app->text_input,
        hermes_scene_keyboard_callback,
        app,
        app->text_buffer,
        HERMES_TEXT_INPUT_MAX,
        true); // empty input is allowed: bare Enter is a real thing to send

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewTextInput);
}

bool hermes_scene_keyboard_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == HermesCustomEventConsoleText) {
        const size_t len = strlen(app->text_buffer);
        if(len > 0) {
            uart_tap_send(app->tap, (const uint8_t*)app->text_buffer, len);
            if(app->local_echo) {
                for(size_t i = 0; i < len; i++) {
                    term_feed_echo(app->term, (uint8_t)app->text_buffer[i]);
                }
            }
        }

        /* A line without its terminator would just sit in the target's input
         * buffer, so the send always finishes the line. */
        uart_tap_send_enter(app->tap, app->enter_mode);
        if(app->local_echo) term_feed_echo(app->term, '\n');

        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void hermes_scene_keyboard_on_exit(void* context) {
    HermesApp* app = context;
    text_input_reset(app->text_input);
}
