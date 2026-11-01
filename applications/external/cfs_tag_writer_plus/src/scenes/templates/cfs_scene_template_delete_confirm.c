#include "src/core/cfs_app_i.h"

static void cfs_template_delete_button(GuiButtonType result, InputType type, void* context) {
    CfsApp* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonErase);
    }
}

void cfs_scene_template_delete_confirm_on_enter(void* context) {
    CfsApp* app = context;
    size_t i = scene_manager_get_scene_state(app->scene_manager, CfsSceneTemplateDeleteConfirm);
    Widget* widget = app->widget;
    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Delete template?");
    if(i < app->template_count) {
        widget_add_text_box_element(
            widget, 4, 14, 120, 34, AlignCenter, AlignTop, app->template_names[i], false);
    }
    // Back keeps it; Right deletes.
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Delete", cfs_template_delete_button, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_template_delete_confirm_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event != CfsCustomEventButtonErase) {
        return false;
    }
    size_t i = scene_manager_get_scene_state(app->scene_manager, CfsSceneTemplateDeleteConfirm);
    if(i < app->template_count) {
        char path[CFS_STORAGE_NAME_MAX + 48];
        cfs_storage_path_in(CFS_TEMPLATE_DIR, app->template_names[i], path, sizeof(path));
        bool ok = cfs_storage_delete(app->storage, path);
        notification_message(app->notifications, ok ? &sequence_success : &sequence_error);
    }
    // Back to the template list (it re-reads the folder), cursor on the first one.
    scene_manager_set_scene_state(app->scene_manager, CfsSceneTemplates, CfsCustomEventMenuBase);
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, CfsSceneTemplates);
    return true;
}

void cfs_scene_template_delete_confirm_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}
