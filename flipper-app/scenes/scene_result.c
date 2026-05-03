#include "scenes.h"

void bitraw_scene_result_on_enter(void* context) {
    BitrawApp* app = context;

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->report));
    view_dispatcher_switch_to_view(app->view_dispatcher, BitrawViewTextBox);
}

bool bitraw_scene_result_on_event(void* context, SceneManagerEvent event) {
    BitrawApp* app = context;
    if(event.type == SceneManagerEventTypeBack) {
        return scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BitrawSceneBrowser);
    }
    return false;
}

void bitraw_scene_result_on_exit(void* context) {
    BitrawApp* app = context;
    text_box_reset(app->text_box);
}
