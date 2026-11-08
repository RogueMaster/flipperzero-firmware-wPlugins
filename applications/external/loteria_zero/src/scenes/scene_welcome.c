#include "scenes.h"

typedef enum {
    WelcomePageTitle = 0,
    WelcomePageDedication1,
    WelcomePageDedication2,
    WelcomePageCount,
} WelcomePage;

static void
    loteria_scene_welcome_button_callback(GuiButtonType result, InputType type, void* context) {
    LoteriaApp* app = context;
    if(type != InputTypeShort) return;
    if(result == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

static void loteria_scene_welcome_show(LoteriaApp* app) {
    widget_reset(app->widget);

    switch(app->welcome_page) {
    case WelcomePageTitle:
        widget_add_string_element(
            app->widget, 64, 10, AlignCenter, AlignTop, FontPrimary, "LOTERIA ZERO");
        widget_add_string_multiline_element(
            app->widget, 64, 28, AlignCenter, AlignTop, FontSecondary, "Tradition in\nyour hands.");
        widget_add_button_element(
            app->widget,
            GuiButtonTypeCenter,
            "Continue",
            loteria_scene_welcome_button_callback,
            app);
        break;
    case WelcomePageDedication1:
        widget_add_string_element(
            app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "For APA");
        widget_add_text_scroll_element(
            app->widget,
            0,
            20,
            128,
            30,
            "The inspiration behind every line of code, every idea, and every project.");
        widget_add_button_element(
            app->widget,
            GuiButtonTypeCenter,
            "Continue",
            loteria_scene_welcome_button_callback,
            app);
        break;
    case WelcomePageDedication2:
        widget_add_text_scroll_element(
            app->widget,
            0,
            10,
            128,
            36,
            "Because the best creations are born from what we love most.");
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Play", loteria_scene_welcome_button_callback, app);
        break;
    default:
        break;
    }
}

void loteria_scene_welcome_on_enter(void* context) {
    LoteriaApp* app = context;
    app->welcome_page = WelcomePageTitle;
    loteria_scene_welcome_show(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, LoteriaViewWidget);
}

bool loteria_scene_welcome_on_event(void* context, SceneManagerEvent event) {
    LoteriaApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(app->welcome_page + 1 < WelcomePageCount) {
            app->welcome_page++;
            loteria_scene_welcome_show(app);
        } else {
            app->persist.skip_welcome = true;
            loteria_app_save(app);
            scene_manager_next_scene(app->scene_manager, LoteriaSceneMainMenu);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = true;
        if(app->welcome_page > WelcomePageTitle) {
            app->welcome_page--;
            loteria_scene_welcome_show(app);
        } else {
            view_dispatcher_stop(app->view_dispatcher);
        }
    }

    return consumed;
}

void loteria_scene_welcome_on_exit(void* context) {
    LoteriaApp* app = context;
    widget_reset(app->widget);
}
