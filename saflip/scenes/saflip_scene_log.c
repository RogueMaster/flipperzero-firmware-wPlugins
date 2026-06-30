#include "../saflip.h"

void saflip_scene_log_dialog_callback(DialogExResult result, void* context) {
    SaflipApp* app = context;

    if(result == DialogExResultLeft) {
        // Return to submenu without modifying anything
        view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewSubmenu);
    } else {
        // Remove selected item
        uint32_t idx = submenu_get_selected_item(app->submenu);
        for(size_t i = idx + 1; i < app->log_entries; i++) {
            app->log[i - 1] = app->log[i];
        }
        app->log_entries--;

        // Redraw and return to submenu
        saflip_scene_log_on_enter(app);
    }
}
void saflip_scene_log_submenu_callback(void* context, InputType type, uint32_t index) {
    SaflipApp* app = context;

    scene_manager_set_scene_state(app->scene_manager, SaflipSceneLog, index);

    if(type == InputTypeShort) {
        scene_manager_set_scene_state(app->scene_manager, SaflipSceneLogInfo, index);
        scene_manager_next_scene(app->scene_manager, SaflipSceneLogInfo);
    } else if(type == InputTypeLong) {
        dialog_ex_set_header(app->dialog, "Remove log entry?", 64, 12, AlignCenter, AlignTop);
        dialog_ex_set_left_button_text(app->dialog, "Cancel");
        dialog_ex_set_right_button_text(app->dialog, "Remove");
        dialog_ex_set_result_callback(app->dialog, saflip_scene_log_dialog_callback);
        dialog_ex_set_context(app->dialog, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewDialog);
    }
}

void saflip_scene_log_on_enter(void* context) {
    SaflipApp* app = context;

    FuriString* label = furi_string_alloc();

    submenu_reset(app->submenu);

    if(app->log_entries == 0) {
        // Can't show 0 log entries
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    for(size_t i = 0; i < app->log_entries; i++) {
        furi_string_printf(
            label,
            "#%d @ %04d-%02d-%02d %02d:%02d",
            i + 1,
            app->log[i].time.year,
            app->log[i].time.month,
            app->log[i].time.day,
            app->log[i].time.hour,
            app->log[i].time.minute);
        submenu_add_item_ex(
            app->submenu, furi_string_get_cstr(label), i, saflip_scene_log_submenu_callback, app);
    }

    furi_string_free(label);

    submenu_set_selected_item(
        app->submenu, scene_manager_get_scene_state(app->scene_manager, SaflipSceneLog));

    view_dispatcher_switch_to_view(app->view_dispatcher, SaflipViewSubmenu);
}

bool saflip_scene_log_on_event(void* context, SceneManagerEvent event) {
    SaflipApp* app = context;
    bool consumed = false;

    UNUSED(event);
    UNUSED(app);

    return consumed;
}

void saflip_scene_log_on_exit(void* context) {
    SaflipApp* app = context;

    UNUSED(app);
}
