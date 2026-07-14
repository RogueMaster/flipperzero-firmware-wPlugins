#include "../fdxb_maker.h"

void fdxb_maker_scene_confirm_nonstandard_country_on_enter(void* context) {
    FdxbMaker* app = context;
    Widget* widget = app->widget;

    widget_add_string_element(
        widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "4-digit Country Value!");
    widget_add_string_multiline_element(
        widget,
        64,
        13,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "Country values above 999 are\n"
        "non-standard, and may not\n"
        "be read correctly everywhere.");

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Cancel", fdxb_maker_widget_callback, context);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "OK", fdxb_maker_widget_callback, context);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewWidget);
}

bool fdxb_maker_scene_confirm_nonstandard_country_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == GuiButtonTypeRight) {
            scene_manager_next_scene(app->scene_manager, FdxbMakerSceneSaveNational);
        } else if(event.event == GuiButtonTypeLeft) {
            scene_manager_previous_scene(app->scene_manager);
        }
    }

    return consumed;
}

void fdxb_maker_scene_confirm_nonstandard_country_on_exit(void* context) {
    FdxbMaker* app = context;

    widget_reset(app->widget);
}
