#include "../fdxb_maker.h"

void fdxb_maker_scene_ask_wipe_trailer_on_enter(void* context) {
    FdxbMaker* app = context;
    Widget* widget = app->widget;

    widget_add_string_element(
        widget, 64, 0, AlignCenter, AlignTop, FontPrimary, "Wipe data block?");
    widget_add_string_multiline_element(
        widget,
        64,
        13,
        AlignCenter,
        AlignTop,
        FontSecondary,
        "There is existing data in\n"
        "the data block. Would you\n"
        "like to remove it?");

    widget_add_button_element(
        widget, GuiButtonTypeLeft, "No", fdxb_maker_widget_callback, context);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Yes", fdxb_maker_widget_callback, context);

    view_dispatcher_switch_to_view(app->view_dispatcher, FdxbMakerViewWidget);
}

bool fdxb_maker_scene_ask_wipe_trailer_on_event(void* context, SceneManagerEvent event) {
    FdxbMaker* app = context;
    FdxbExpanded* data = app->data;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == GuiButtonTypeRight) {
            data->trailer = 0;
            data->temperature = 0;

            fdxb_maker_save_key_and_switch_scenes(app);
        } else if(event.event == GuiButtonTypeLeft) {
            fdxb_maker_save_key_and_switch_scenes(app);
        }
    }

    return consumed;
}

void fdxb_maker_scene_ask_wipe_trailer_on_exit(void* context) {
    FdxbMaker* app = context;

    widget_reset(app->widget);
}
