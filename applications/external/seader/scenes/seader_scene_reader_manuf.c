#include "../seader_i.h"

#define TAG "Seader:Scene:ReaderManuf"

void seader_scene_reader_manuf_text_input_callback(void* context) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventTextInputDone);
}

void seader_scene_reader_manuf_on_enter(void* context) {
    Seader* seader = context;

    TextInput* text_input = seader_get_text_input(seader);
    if(!text_input) {
        FURI_LOG_E(TAG, "Text input view unavailable");
        return;
    }

    strlcpy(seader->reader_manuf_edit, seader->reader_manufacturer, SEADER_READER_NAME_MAX);

    text_input_set_header_text(text_input, "Manufacturer (may be empty)");
    text_input_set_result_callback(
        text_input,
        seader_scene_reader_manuf_text_input_callback,
        seader,
        seader->reader_manuf_edit,
        SEADER_READER_NAME_MAX,
        false); // prefilled with the current manufacturer

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewTextInput);
}

bool seader_scene_reader_manuf_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeaderCustomEventTextInputDone) {
            // Step 1 of 2: manufacturer captured in reader_manuf_edit; now the
            // product name. The final save (with PID bump) happens there.
            scene_manager_next_scene(seader->scene_manager, SeaderSceneReaderName);
            consumed = true;
        }
    }

    return consumed;
}

void seader_scene_reader_manuf_on_exit(void* context) {
    Seader* seader = context;
    if(seader->text_input) {
        text_input_reset(seader->text_input);
    }
}
