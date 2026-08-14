#include "../uk_mbirth_sonicare.h"
#include <gui/canvas.h>
#include <gui/modules/widget.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>
#include <uk_mbirth_sonicare_icons.h>
#include <dolphin/dolphin.h>

static void sonicare_scene_reset_confirm_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    Sonicare* app = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void sonicare_scene_reset_confirm_on_enter(void* context) {
    Sonicare* app = context;
    Widget* widget = app->widget;

    widget_reset(widget);

    widget_add_string_element(
        widget, 64, 2, AlignCenter, AlignTop, FontPrimary, "Reset usage counter?");
    widget_add_text_box_element(
        widget,
        0,
        14,
        128,
        38,
        AlignLeft,
        AlignTop,
        "\e#Warning:\e# The NTAG213 per-\nmanently locks after 3 wrong\npassword attempts.",
        false);

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Cancel", sonicare_scene_reset_confirm_widget_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Reset", sonicare_scene_reset_confirm_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, SonicareViewWidget);
}

bool sonicare_scene_reset_confirm_on_event(void* context, SceneManagerEvent event) {
    Sonicare* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(app->scene_manager, SonicareSceneReset);
            consumed = true;
        } else if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(app->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(app->scene_manager);
    }

    return consumed;
}

void sonicare_scene_reset_confirm_on_exit(void* context) {
    Sonicare* app = context;
    widget_reset(app->widget);
}
