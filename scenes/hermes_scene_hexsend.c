#include "../hermes_i.h"

/* Send raw bytes the keyboard cannot compose: a binary command, a magic packet,
 * an exact escape sequence.
 *
 * byte_input edits a *fixed-length* field, so this is a two-phase scene: first
 * number_input picks how many bytes, then byte_input fills them. The phase is
 * kept in the scene state so one scene drives both widgets. */

typedef enum {
    HexPhaseLength,
    HexPhaseBytes,
} HexPhase;

static void hermes_scene_hexsend_length_cb(void* context, int32_t number) {
    HermesApp* app = context;
    app->hex_len = (uint8_t)number;
    view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventBaudEntered);
}

static void hermes_scene_hexsend_bytes_cb(void* context) {
    HermesApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, HermesCustomEventHexEntered);
}

/** Present the hex grid for the current hex_len. */
static void hermes_scene_hexsend_show_bytes(HermesApp* app) {
    scene_manager_set_scene_state(app->scene_manager, HermesSceneHexSend, HexPhaseBytes);
    byte_input_set_result_callback(
        app->byte_input,
        hermes_scene_hexsend_bytes_cb,
        NULL,
        app,
        app->hex_buffer,
        app->hex_len);
    byte_input_set_header_text(app->byte_input, "Bytes to send");
    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewByteInput);
}

void hermes_scene_hexsend_on_enter(void* context) {
    HermesApp* app = context;

    if(app->hex_len == 0) app->hex_len = 2; // sensible first count

    scene_manager_set_scene_state(app->scene_manager, HermesSceneHexSend, HexPhaseLength);
    number_input_set_header_text(app->number_input, "How many bytes?");
    number_input_set_result_callback(
        app->number_input,
        hermes_scene_hexsend_length_cb,
        app,
        app->hex_len,
        1,
        HERMES_HEX_MAX);
    view_dispatcher_switch_to_view(app->view_dispatcher, HermesViewNumberInput);
}

bool hermes_scene_hexsend_on_event(void* context, SceneManagerEvent event) {
    HermesApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == HermesCustomEventBaudEntered) {
            /* length chosen -> second phase, same scene */
            hermes_scene_hexsend_show_bytes(app);
            return true;
        }
        if(event.event == HermesCustomEventHexEntered) {
            uart_tap_send(app->tap, app->hex_buffer, app->hex_len);
            if(app->local_echo) {
                for(uint8_t i = 0; i < app->hex_len; i++) {
                    term_feed_echo(app->term, app->hex_buffer[i]);
                }
            }
            session_log_note(app->log, "hex sent");
            hermes_notify_blip(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, HermesSceneConsole);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        /* From the byte grid, Back steps to the length picker rather than
         * leaving outright - one Back, one phase. */
        if(scene_manager_get_scene_state(app->scene_manager, HermesSceneHexSend) ==
           HexPhaseBytes) {
            hermes_scene_hexsend_on_enter(app);
            return true;
        }
    }

    return false;
}

void hermes_scene_hexsend_on_exit(void* context) {
    UNUSED(context);
}
