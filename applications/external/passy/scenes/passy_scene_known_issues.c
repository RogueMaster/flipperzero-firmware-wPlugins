#include "../passy_i.h"

static const char* known_issues_text = "Passy only uses BAC, not PACE for auth\n"
                                       "PACE-only:\n"
                                       "  - German Personalausweis\n"
                                       "  - German Aufenthaltstitel post 2015\n"
                                       "  - Dutch passport post 2022\n";

void passy_scene_known_issues_on_enter(void* context) {
    Passy* passy = context;

    furi_string_reset(passy->text_box_store);

    FuriString* str = passy->text_box_store;
    furi_string_cat_printf(str, "%s\n", known_issues_text);

    text_box_set_font(passy->text_box, TextBoxFontText);
    text_box_set_text(passy->text_box, furi_string_get_cstr(passy->text_box_store));
    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewTextBox);
}

bool passy_scene_known_issues_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(passy->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(passy->scene_manager);
    }
    return consumed;
}

void passy_scene_known_issues_on_exit(void* context) {
    Passy* passy = context;

    // Clear views
    text_box_reset(passy->text_box);
}
