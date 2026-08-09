#include "../uk_mbirth_sonicare.h"
#include <gui/canvas.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <uk_mbirth_sonicare_icons.h>
#include <dolphin/dolphin.h>

static void sonicare_scene_reset_complete_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Sonicare* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void sonicare_scene_reset_complete_on_enter(void* context) {
    Sonicare* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    if(app->reset_state == SonicareResetStateSuccess) {
        widget_add_icon_element(widget, 0, 0, &I_sonicare_brush);
        widget_add_string_element(
            widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Counter reset!");
        widget_add_string_element(
            widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "Read again to verify");
    } else {
        const char* reason = (app->reset_state == SonicareResetStateFailedAuth) ? "Auth failed" :
                                                                                  "Write failed";
        widget_add_string_element(
            widget, 64, 20, AlignCenter, AlignCenter, FontPrimary, "Reset failed");
        widget_add_string_element(widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, reason);
    }

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Back", sonicare_scene_reset_complete_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewWidget);
}

bool sonicare_scene_reset_complete_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // Go back to start menu so user can re-read the head
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, SonicareSceneStart);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SonicareSceneStart);
        consumed = true;
    }

    return consumed;
}

void sonicare_scene_reset_complete_on_exit(void* context) {
    Sonicare* app = context;
    widget_reset(app->widget);
}
