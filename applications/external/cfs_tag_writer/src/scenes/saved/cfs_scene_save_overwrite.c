#include "src/core/cfs_app_i.h"

static void cfs_save_overwrite_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    CfsApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonOverwrite);
    }
}

void cfs_scene_save_overwrite_on_enter(void* context) {
    CfsApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Name Exists");
    // Show the actual filename that will be overwritten, extension included
    // (save_name itself is stored without it - cfs_storage.c appends it).
    char display_name[CFS_STORAGE_NAME_MAX + sizeof(CFS_STORAGE_EXTENSION)];
    snprintf(display_name, sizeof(display_name), "%s%s", app->save_name, CFS_STORAGE_EXTENSION);
    // save_name can run up to CFS_STORAGE_NAME_MAX (64) chars (it's built from
    // brand + material + color + serial) - a plain centered string element
    // doesn't wrap, so long names ran off both edges of the screen. The text
    // box element word-wraps and keeps the centered look of the rest of this
    // screen (unlike the scroll element, which is left-aligned/paragraph-style).
    // x=4/width=120 leaves a small margin instead of touching both edges.
    widget_add_text_box_element(
        widget, 4, 12, 120, 26, AlignCenter, AlignTop, display_name, false);
    widget_add_string_element(
        widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Overwrite it?");
    // Back cancels (re-edit the name); Right confirms the overwrite.
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Overwrite", cfs_save_overwrite_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_save_overwrite_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == CfsCustomEventButtonOverwrite) {
        cfs_app_save_pending(app);
        // Pop both this confirm and the save-name scene, back to the invoker.
        CfsScene invoker = app->save_from_write ? CfsSceneWriteConfirm : CfsSceneReadResult;
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, invoker);
        return true;
    }
    return false;
}

void cfs_scene_save_overwrite_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}
