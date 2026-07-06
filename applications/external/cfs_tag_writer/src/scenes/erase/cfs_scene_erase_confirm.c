#include "src/core/cfs_app_i.h"

static void cfs_erase_confirm_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    CfsApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CfsCustomEventButtonErase);
    }
}

void cfs_scene_erase_confirm_on_enter(void* context) {
    CfsApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Erase Tag");
    widget_add_string_element(
        widget, 64, 22, AlignCenter, AlignCenter, FontSecondary, "Wipes CFS data and");
    widget_add_string_element(
        widget, 64, 34, AlignCenter, AlignCenter, FontSecondary, "resets keys to factory.");
    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Erase", cfs_erase_confirm_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, CfsViewWidget);
}

bool cfs_scene_erase_confirm_on_event(void* context, SceneManagerEvent event) {
    CfsApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == CfsCustomEventButtonErase) {
        scene_manager_next_scene(app->scene_manager, CfsSceneEraseProgress);
        return true;
    }
    return false;
}

void cfs_scene_erase_confirm_on_exit(void* context) {
    CfsApp* app = context;
    widget_reset(app->widget);
}
