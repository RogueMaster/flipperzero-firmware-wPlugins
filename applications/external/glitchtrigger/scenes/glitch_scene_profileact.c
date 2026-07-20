#include "../glitch_trigger_i.h"

/* Action screen for a picked profile: Load it into the live config, or Delete
 * it. Both return to the profiles list. */

typedef enum {
    ProfileActLoad = 1,
    ProfileActDelete,
} ProfileActEvent;

static void glitch_scene_profileact_button_cb(GuiButtonType result, InputType type, void* context) {
    GlitchApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ProfileActLoad);
    } else if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ProfileActDelete);
    }
}

void glitch_scene_profileact_on_enter(void* context) {
    GlitchApp* app = context;
    Widget* widget = app->widget;
    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 6, AlignCenter, AlignTop, FontPrimary, app->selected_profile);
    widget_add_string_multiline_element(
        widget,
        64,
        26,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Load into the live config,\nor delete this profile.");
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Delete", glitch_scene_profileact_button_cb, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Load", glitch_scene_profileact_button_cb, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, GlitchViewWidget);
}

bool glitch_scene_profileact_on_event(void* context, SceneManagerEvent event) {
    GlitchApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == ProfileActLoad) {
        if(glitch_storage_load_profile(app->selected_profile, &app->params)) {
            glitch_notify_arm(app);
        } else {
            glitch_notify_disarm(app);
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(event.event == ProfileActDelete) {
        glitch_storage_delete_profile(app->selected_profile);
        glitch_notify_disarm(app);
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void glitch_scene_profileact_on_exit(void* context) {
    GlitchApp* app = context;
    widget_reset(app->widget);
}
