#include "kyberwrite_scene.h"

static void hint_button_callback(GuiButtonType result, InputType type, void* context);

static void draw_hint(KyberApp* app) {
    widget_reset(app->hint_widget);
    widget_add_string_element(app->hint_widget, 64, 8, AlignCenter, AlignTop, FontPrimary, app->hint_title);
    uint8_t y = 24;
    for(uint8_t i = 0; i < app->hint_text_lines; i++) {
        widget_add_string_element(app->hint_widget, 8, y, AlignLeft, AlignTop, FontSecondary, app->hint_text[i]);
        y += 10;
    }
    widget_add_button_element(app->hint_widget, GuiButtonTypeRight, "Next", hint_button_callback, app);
}

static void hint_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    KyberApp* app = context;
    if(result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, KyberEventHintNext);
    }
}

void kyberwrite_scene_hint_on_enter(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    draw_hint(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, KyberViewHint);
}

bool kyberwrite_scene_hint_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    KyberApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == KyberEventHintNext) {
            scene_manager_next_scene(app->scene_manager, app->hint_next_scene);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void kyberwrite_scene_hint_on_exit(void* context) {
    furi_assert(context);
    KyberApp* app = context;
    widget_reset(app->hint_widget);
}


