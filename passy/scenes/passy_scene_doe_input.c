#include "../passy_i.h"
#include <gui/modules/validators.h>

#define PASSY_APP_FILE_PREFIX "Passy"

void passy_scene_doe_input_text_input_callback(void* context) {
    Passy* passy = context;

    view_dispatcher_send_custom_event(passy->view_dispatcher, PassyCustomEventTextInputDone);
}

void passy_scene_doe_input_on_enter(void* context) {
    Passy* passy = context;

    // Setup view
    TextInput* text_input = passy->text_input;

    text_input_set_header_text(text_input, "DoE: YYMMDD");
    text_input_set_minimum_length(text_input, 6);
    if(passy->date_of_expiry[0] != '\0') {
        strlcpy(passy->text_store, passy->date_of_expiry, sizeof(passy->text_store));
    }
    text_input_set_result_callback(
        text_input,
        passy_scene_doe_input_text_input_callback,
        passy,
        passy->text_store,
        sizeof(passy->text_store),
        false);

    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewTextInput);
}

bool passy_scene_doe_input_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == PassyCustomEventTextInputDone) {
            strlcpy(passy->date_of_expiry, passy->text_store, sizeof(passy->date_of_expiry));
            passy_save_mrz_info(passy);
            scene_manager_next_scene(passy->scene_manager, PassySceneMainMenu);
            consumed = true;
        }
    }
    return consumed;
}

void passy_scene_doe_input_on_exit(void* context) {
    Passy* passy = context;

    // Clear view
    memset(passy->text_store, 0, sizeof(passy->text_store));
    text_input_reset(passy->text_input);
}
