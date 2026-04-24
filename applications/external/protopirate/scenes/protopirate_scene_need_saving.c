// scenes/protopirate_scene_need_saving.c
#include "../protopirate_app_i.h"
#include "proto_pirate_icons.h"

#define TAG "ProtoPirateNeedSaving"

static void
    protopirate_scene_need_saving_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    ProtoPirateApp* app = context;

    if((result == GuiButtonTypeRight) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ProtoPirateCustomEventSceneStay);
    } else if((result == GuiButtonTypeLeft) && (type == InputTypeShort)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ProtoPirateCustomEventSceneExit);
    }
}

void protopirate_scene_need_saving_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;

    if(!protopirate_ensure_widget(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    widget_add_icon_element(app->widget, 0, 12, &I_WarningDolphin_45x42);
    widget_add_string_multiline_element(
        app->widget, 86, 2, AlignCenter, AlignTop, FontPrimary, "Exit to\nMain Menu?");
    widget_add_string_multiline_element(
        app->widget,
        86,
        26,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "All unsaved data\nwill be lost!");

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Stay", protopirate_scene_need_saving_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Exit", protopirate_scene_need_saving_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
}

bool protopirate_scene_need_saving_on_event(void* context, SceneManagerEvent event) {
    furi_check(context);
    ProtoPirateApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        // Hardware back button = same as "Stay"
        scene_manager_previous_scene(app->scene_manager);
        return true;
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventSceneStay) {
            scene_manager_previous_scene(app->scene_manager);
            return true;
        } else if(event.event == ProtoPirateCustomEventSceneExit) {
            protopirate_release_shared_radio_state(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, ProtoPirateSceneStart);
            return true;
        }
    }
    return false;
}

void protopirate_scene_need_saving_on_exit(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    widget_reset(app->widget);
}
