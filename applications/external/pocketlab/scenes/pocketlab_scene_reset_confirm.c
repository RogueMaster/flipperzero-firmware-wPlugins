#include "../pocketlab_i.h"

static void pocketlab_scene_reset_confirm_button_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    PocketLab* app = context;
    if(type != InputTypeShort) {
        return;
    }
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PocketLabCustomEventResetCancel);
    } else if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PocketLabCustomEventResetConfirm);
    }
}

void pocketlab_scene_reset_confirm_on_enter(void* context) {
    PocketLab* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 12, AlignCenter, AlignTop, FontPrimary, "Reset progress?");
    widget_add_string_multiline_element(
        widget, 64, 30, AlignCenter, AlignTop, FontSecondary, "All XP and badges\nwill be lost.");
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Cancel", pocketlab_scene_reset_confirm_button_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Reset", pocketlab_scene_reset_confirm_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, PocketLabViewWidget);
}

bool pocketlab_scene_reset_confirm_on_event(void* context, SceneManagerEvent event) {
    PocketLab* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PocketLabCustomEventResetConfirm) {
            pocketlab_reset_progress(app);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        if(event.event == PocketLabCustomEventResetCancel) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }

    return false;
}

void pocketlab_scene_reset_confirm_on_exit(void* context) {
    PocketLab* app = context;
    widget_reset(app->widget);
}
