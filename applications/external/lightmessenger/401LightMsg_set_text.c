/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#include "401LightMsg_set_text.h"
#include "401LightMsg_main.h"

static const char* TAG = "401_LightMsgSetText";
/**
 * Callback function for the "set text" scene. Triggers a custom event to save the input text.
 *
 * @param ctx The application ctx.
 */
void app_scene_set_text_callback(void* ctx) {
    furi_assert(ctx);
    AppContext* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, SetTextInputSaveEvent);
}

/**
 * Handles the entry into the "set text" scene. Configures the text input view
 * and switches to it.
 *
 * @param ctx The application ctx.
 */
void app_scene_set_text_on_enter(void* ctx) {
    AppContext* app = ctx;
    Configuration* light_msg_data = (Configuration*)app->data->config;

    text_input_reset(app->sceneSetText);
    text_input_set_header_text(app->sceneSetText, "Set the text to display");
    text_input_set_result_callback(
        app->sceneSetText,
        app_scene_set_text_callback,
        app,
        light_msg_data->text,
        LIGHTMSG_MAX_TEXT_LEN,
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewSetText);
}

/**
 * Handles events for the "set text" scene. Specifically, checks for the
 * custom event indicating that the text input should be saved.
 *
 * @param ctx The application ctx.
 * @param event The scene manager event.
 * @return true if the event was consumed, false otherwise.
 */
bool app_scene_set_text_on_event(void* ctx, SceneManagerEvent event) {
    furi_assert(ctx);
    AppContext* app = ctx;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SetTextInputSaveEvent) {
            Configuration* light_msg_data = (Configuration*)app->data->config;
            char* text = light_msg_data->text;
            // Replace "_" with spaces
            while(*text) {
                if(*text == '_') {
                    *text = ' ';
                }
                text++;
            }

            l401_err res = config_save_json(LIGHTMSGCONF_CONFIG_FILE, light_msg_data);
            if(res == L401_OK) {
                FURI_LOG_I(
                    TAG, "Successfully saved configuration to %s.", LIGHTMSGCONF_CONFIG_FILE);
            } else {
                FURI_LOG_E(TAG, "Error while saving to %s: %d", LIGHTMSGCONF_CONFIG_FILE, res);
            }
            scene_manager_next_scene(app->scene_manager, AppSceneAcc);
            consumed = true;
        }
    }
    return consumed;
}

/**
 * Handles the exit from the "set text" scene. No specific actions are
 * currently performed during exit.
 *
 * @param ctx The application ctx.
 */
void app_scene_set_text_on_exit(void* ctx) {
    UNUSED(ctx);
}
