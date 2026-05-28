#include "droidbeacon_scene.h"

static void splash_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    DroidbeaconApp* app = context;
    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DroidbeaconEventSplashNext);
    }
}

void droidbeacon_scene_splash_on_enter(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;

    widget_reset(app->splash_widget);
    widget_add_string_element(app->splash_widget, 64, 0,  AlignCenter, AlignTop, FontPrimary,   "Droid Beacon");
    widget_add_string_element(app->splash_widget, 8,  12, AlignLeft,   AlignTop, FontSecondary, "Emulate Galaxy's Edge");
    widget_add_string_element(app->splash_widget, 8,  22, AlignLeft,   AlignTop, FontSecondary, "park location beacons");
    widget_add_string_element(app->splash_widget, 8,  32, AlignLeft,   AlignTop, FontSecondary, "for Droid Depot droids");
    widget_add_string_element(app->splash_widget, 8,  42, AlignLeft,   AlignTop, FontSecondary, "DBAD: Don't use at Disney");
    widget_add_button_element(app->splash_widget, GuiButtonTypeRight, "Next", splash_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, DroidbeaconViewSplash);
}

bool droidbeacon_scene_splash_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DroidbeaconEventSplashNext) {
            scene_manager_next_scene(app->scene_manager, DroidbeaconSceneMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void droidbeacon_scene_splash_on_exit(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    widget_reset(app->splash_widget);
}
