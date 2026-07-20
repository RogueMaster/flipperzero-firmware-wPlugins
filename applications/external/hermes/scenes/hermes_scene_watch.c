#include "../hermes_i.h"
#include <string.h>

/* Arm a string to watch for. The point is to stop staring at the screen: set
 * "login:" or "Password", start the board, and let the Flipper buzz you. */

static void hermes_scene_watch_callback(void* context) {
    HermesApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventTriggerSet);
}

void hermes_scene_watch_on_enter(void* context) {
    HermesApp* app = context;

    /* Seed with whatever is armed, so editing it is easy and clearing it is
     * just deleting the text. */
    strncpy(app->text_buffer, trigger_pattern(app->trigger), HERMES_TEXT_INPUT_MAX - 1);
    app->text_buffer[HERMES_TEXT_INPUT_MAX - 1] = '\0';

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Buzz when this appears");
    text_input_set_result_callback(
        app->text_input,
        hermes_scene_watch_callback,
        app,
        app->text_buffer,
        TRIGGER_PATTERN_MAX,
        true); // empty is allowed - that is how you disarm

    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewTextInput);
}

bool hermes_scene_watch_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == HermesCustomEventTriggerSet) {
        trigger_set(app->trigger, app->text_buffer);
        console_view_set_watch(app->console_view, trigger_pattern(app->trigger));

        if(trigger_is_armed(app->trigger)) {
            char note[TRIGGER_PATTERN_MAX + 24];
            snprintf(note, sizeof(note), "watching for '%s'", trigger_pattern(app->trigger));
            session_log_note(app->log, note);
        }

        /* Back past the Ctrl palette to the console, which is where the user
         * was and where the watch takes effect. */
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, HermesSceneConsole);
        return true;
    }

    return false;
}

void hermes_scene_watch_on_exit(void* context) {
    HermesApp* app = context;
    text_input_reset(app->text_input);
}
