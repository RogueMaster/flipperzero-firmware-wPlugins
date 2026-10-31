#include "flippass_scene_text_view.h"

#include "../flippass.h"
#include "flippass_scene.h"
#include "flippass_scene_other_field_actions.h"

#include <stdio.h>

enum {
    FlipPassSceneTextViewEventTypeBluetooth = 0x300U,
    FlipPassSceneTextViewEventRefresh,
    FlipPassSceneTextViewEventTypeUsb,
    FlipPassSceneTextViewEventRunPendingAction,
};

static void flippass_scene_text_view_button_callback(
    GuiButtonType button_type,
    InputType type,
    void* context) {
    App* app = context;

    if(type != InputTypeShort) {
        return;
    }

    if(button_type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FlipPassSceneTextViewEventTypeBluetooth);
    } else if(button_type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassSceneTextViewEventRefresh);
    } else if(button_type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, FlipPassSceneTextViewEventTypeUsb);
    }
}

void flippass_scene_text_view_show(
    App* app,
    const char* title,
    const char* body,
    uint32_t return_scene) {
    furi_assert(app);

    snprintf(app->text_view_title, sizeof(app->text_view_title), "%s", title != NULL ? title : "");
    furi_string_set_str(app->text_view_body, body != NULL ? body : "");
    app->text_view_return_scene = return_scene;
}

static void flippass_scene_text_view_build_widget(App* app) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 4, AlignCenter, AlignTop, FontPrimary, app->text_view_title);
    widget_add_text_scroll_element(
        app->widget, 0, 14, 128, 49, furi_string_get_cstr(app->text_view_body));
    if(app->other_field_show_active) {
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "BT", flippass_scene_text_view_button_callback, app);
        if(app->other_field_show_totp_refresh) {
            widget_add_button_element(
                app->widget,
                GuiButtonTypeCenter,
                "Refresh",
                flippass_scene_text_view_button_callback,
                app);
        }
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "USB", flippass_scene_text_view_button_callback, app);
    }
}

void flippass_scene_text_view_on_enter(void* context) {
    App* app = context;

    flippass_ui_ensure_widget(app);
    flippass_scene_text_view_build_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewWidget);
}

bool flippass_scene_text_view_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeTick && flippass_other_field_show_refresh_due(app)) {
        flippass_other_field_refresh_visible_show(app, app->text_view_return_scene);
        flippass_scene_text_view_build_widget(app);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom && app->other_field_show_active) {
        if(event.event == FlipPassSceneTextViewEventTypeBluetooth) {
            flippass_other_field_begin_type_action(
                app, true, FlipPassSceneTextViewEventRunPendingAction);
            return true;
        }

        if(event.event == FlipPassSceneTextViewEventRefresh) {
            flippass_other_field_refresh_visible_show(app, app->text_view_return_scene);
            flippass_scene_text_view_build_widget(app);
            return true;
        }

        if(event.event == FlipPassSceneTextViewEventTypeUsb) {
            flippass_other_field_begin_type_action(
                app, false, FlipPassSceneTextViewEventRunPendingAction);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == FlipPassSceneTextViewEventRunPendingAction) {
        flippass_other_field_run_pending_type_action(app, app->text_view_return_scene);
        return true;
    }

    if(event.type == SceneManagerEventTypeBack) {
        if(!scene_manager_search_and_switch_to_previous_scene(
               app->scene_manager, app->text_view_return_scene)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }

    return false;
}

void flippass_scene_text_view_on_exit(void* context) {
    App* app = context;
    flippass_other_field_clear_show_state(app);
    if(app->widget != NULL) {
        widget_reset(app->widget);
    }
}
