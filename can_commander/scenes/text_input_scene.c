#include "../can_commander.h"

#include <stdio.h>

static void cancommander_scene_text_input_done(void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventInputDone);
}

static void cancommander_scene_byte_input_done(void* context) {
    App* app = context;

    if(app->input_hex_mode == AppHexInputU8 && app->input_hex_count >= 1U) {
        snprintf(app->input_work, sizeof(app->input_work), "%02X", app->input_hex_store[0]);
    } else if(app->input_hex_mode == AppHexInputU16 && app->input_hex_count >= 2U) {
        uint32_t value = ((uint32_t)app->input_hex_store[0] << 8) | ((uint32_t)app->input_hex_store[1]);
        value &= 0x7FFUL;
        snprintf(app->input_work, sizeof(app->input_work), "%lX", (unsigned long)value);
    } else if(app->input_hex_mode == AppHexInputU32 && app->input_hex_count >= 4U) {
        const uint32_t value = ((uint32_t)app->input_hex_store[0] << 24) |
                               ((uint32_t)app->input_hex_store[1] << 16) |
                               ((uint32_t)app->input_hex_store[2] << 8) |
                               ((uint32_t)app->input_hex_store[3]);
        snprintf(app->input_work, sizeof(app->input_work), "%lX", (unsigned long)value);
    } else if(app->input_hex_mode == AppHexInputBytes && app->input_hex_count > 0U) {
        size_t pos = 0U;
        app->input_work[0] = '\0';

        for(uint8_t i = 0; i < app->input_hex_count; i++) {
            if(pos + 3U >= sizeof(app->input_work)) {
                break;
            }
            const int wrote = snprintf(
                app->input_work + pos,
                sizeof(app->input_work) - pos,
                "%02X",
                app->input_hex_store[i]);
            if(wrote <= 0) {
                break;
            }
            pos += (size_t)wrote;
        }
    }

    view_dispatcher_send_custom_event(app->view_dispatcher, AppCustomEventInputDone);
}

void cancommander_scene_text_input_on_enter(void* context) {
    App* app = context;

    if(app->input_use_byte_input && app->input_hex_count > 0U) {
        byte_input_set_header_text(
            app->byte_input, app->input_header ? app->input_header : "Edit Hex Value");
        byte_input_set_result_callback(
            app->byte_input,
            cancommander_scene_byte_input_done,
            NULL,
            app,
            app->input_hex_store,
            app->input_hex_count);
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewByteInput);
        return;
    }

    text_input_reset(app->text_input);
    text_input_set_result_callback(
        app->text_input,
        cancommander_scene_text_input_done,
        app,
        app->input_work,
        sizeof(app->input_work),
        true);

    text_input_set_header_text(app->text_input, app->input_header ? app->input_header : "Edit Args");

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewTextInput);
}

bool cancommander_scene_text_input_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == AppCustomEventInputDone) {
        app_apply_edit(app);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void cancommander_scene_text_input_on_exit(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    byte_input_set_result_callback(app->byte_input, NULL, NULL, NULL, NULL, 0);
    byte_input_set_header_text(app->byte_input, "");
}
