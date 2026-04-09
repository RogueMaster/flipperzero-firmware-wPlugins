/*
 * Text input scene.
 */

#include "../tagtinker_app.h"

static void text_input_done_cb(void* ctx) {
    TagTinkerApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static void text_input_sanitize_name(char* value) {
    if(!value) return;

    for(char* p = value; *p; p++) {
        if(*p == '|' || *p == '\r' || *p == '\n') {
            *p = ' ';
        }
    }
}

void tagtinker_scene_text_input_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;
    uint32_t mode = scene_manager_get_scene_state(app->scene_manager, TagTinkerSceneTextInput);
    bool rename_target = mode == TagTinkerTextInputRenameTarget;
    bool clear = mode == TagTinkerTextInputNewText;

    if(rename_target) {
        if(app->selected_target >= 0 && app->selected_target < app->target_count) {
            strncpy(
                app->text_input_buf,
                app->targets[app->selected_target].name,
                sizeof(app->text_input_buf) - 1U);
            app->text_input_buf[sizeof(app->text_input_buf) - 1U] = '\0';
        } else {
            memset(app->text_input_buf, 0, sizeof(app->text_input_buf));
        }
    } else if(clear) {
        memset(app->text_input_buf, 0, sizeof(app->text_input_buf));
        scene_manager_set_scene_state(
            app->scene_manager, TagTinkerSceneTextInput, TagTinkerTextInputKeepText);
    }

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, rename_target ? "Target name:" : "Text to display:");
    text_input_set_result_callback(
        app->text_input,
        text_input_done_cb,
        app,
        app->text_input_buf,
        sizeof(app->text_input_buf),
        clear && !rename_target);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewTextInput);
}

bool tagtinker_scene_text_input_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;
    if(event.type != SceneManagerEventTypeCustom) return false;

    uint32_t mode = scene_manager_get_scene_state(app->scene_manager, TagTinkerSceneTextInput);
    if(mode == TagTinkerTextInputRenameTarget) {
        if(app->selected_target >= 0 && app->selected_target < app->target_count) {
            TagTinkerTarget* target = &app->targets[app->selected_target];
            text_input_sanitize_name(app->text_input_buf);
            if(strlen(app->text_input_buf) == 0U) {
                tagtinker_target_set_default_name(target);
            } else {
                strncpy(target->name, app->text_input_buf, TAGTINKER_TARGET_NAME_LEN);
                target->name[TAGTINKER_TARGET_NAME_LEN] = '\0';
            }
            tagtinker_targets_save(app);
        }

        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, TagTinkerSceneTargetActions);
        return true;
    }

    if(strlen(app->text_input_buf) == 0) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, TagTinkerSceneTargetActions);
        return true;
    }

    /* Configure settings (Add Preset flow) */
    scene_manager_next_scene(app->scene_manager, TagTinkerSceneSizePicker);
    return true;
}

void tagtinker_scene_text_input_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;
    text_input_reset(app->text_input);
}
