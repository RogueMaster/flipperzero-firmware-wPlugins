#include "kyberwrite_scene.h"

static void splash_button_callback(GuiButtonType result, InputType type, void* context);

static void draw_splash(KyberApp* app) {
    widget_reset(app->splash_widget);
    widget_add_string_element(app->splash_widget, 64, 8,  AlignCenter, AlignTop, FontPrimary,   "Kyber Write");
    widget_add_string_element(app->splash_widget, 8,  24, AlignLeft,   AlignTop, FontSecondary, "Programmer for Disney");
    widget_add_string_element(app->splash_widget, 8,  34, AlignLeft,   AlignTop, FontSecondary, "Lightsaber Kyber Crystals");
    widget_add_button_element(app->splash_widget, GuiButtonTypeRight, "Next", splash_button_callback, app);
    const char* hints_label = app->show_hints ? "[X] Hints" : "[  ] Hints";
    widget_add_button_element(app->splash_widget, GuiButtonTypeCenter, hints_label, splash_button_callback, app);
}

static void splash_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    KyberApp* app = context;
    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, KyberEventSplashNext);
    } else if(result == GuiButtonTypeCenter) {
        app->show_hints = !app->show_hints;
        draw_splash(app);
    }
}
void kyberwrite_scene_splash_on_enter(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    draw_splash(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, KyberViewSplash);
}

bool kyberwrite_scene_splash_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KyberApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == KyberEventSplashNext) {
            scene_manager_next_scene(app->scene_manager, KyberSceneMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        view_dispatcher_stop(app->view_dispatcher);
        consumed = true;
    }

    return consumed;
}

void kyberwrite_scene_splash_on_exit(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    widget_reset(app->splash_widget);
}


