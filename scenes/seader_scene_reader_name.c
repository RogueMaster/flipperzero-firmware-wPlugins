#include "../seader_i.h"

#define TAG "Seader:Scene:ReaderName"

void seader_scene_reader_name_text_input_callback(void* context) {
    Seader* seader = context;
    view_dispatcher_send_custom_event(seader->view_dispatcher, SeaderCustomEventTextInputDone);
}

void seader_scene_reader_name_on_enter(void* context) {
    Seader* seader = context;

    TextInput* text_input = seader_get_text_input(seader);
    if(!text_input) {
        FURI_LOG_E(TAG, "Text input view unavailable");
        return;
    }

    strlcpy(seader->reader_name_edit, seader->reader_product, SEADER_READER_NAME_MAX);

    text_input_set_header_text(text_input, "Product name");
    text_input_set_result_callback(
        text_input,
        seader_scene_reader_name_text_input_callback,
        seader,
        seader->reader_name_edit,
        SEADER_READER_NAME_MAX,
        false); // prefilled with the current name

    view_dispatcher_switch_to_view(seader->view_dispatcher, SeaderViewTextInput);
}

bool seader_scene_reader_name_on_event(void* context, SceneManagerEvent event) {
    Seader* seader = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SeaderCustomEventTextInputDone) {
            // Step 2 of 2: commit both manufacturer + product. Bump the USB PID
            // if either changed so Windows re-enumerates a fresh device node
            // instead of showing the cached old name.
            bool changed =
                strncmp(
                    seader->reader_manuf_edit,
                    seader->reader_manufacturer,
                    SEADER_READER_NAME_MAX) != 0 ||
                strncmp(
                    seader->reader_name_edit, seader->reader_product, SEADER_READER_NAME_MAX) != 0;
            if(changed) {
                seader->reader_pid++;
                if(seader->reader_pid == 0) {
                    seader->reader_pid = SEADER_READER_DEFAULT_PID;
                }
            }
            strlcpy(
                seader->reader_manufacturer, seader->reader_manuf_edit, SEADER_READER_NAME_MAX);
            strlcpy(seader->reader_product, seader->reader_name_edit, SEADER_READER_NAME_MAX);
            seader_reader_settings_save(seader);
            scene_manager_search_and_switch_to_previous_scene(
                seader->scene_manager, SeaderSceneSamPresent);
            consumed = true;
        }
    }

    return consumed;
}

void seader_scene_reader_name_on_exit(void* context) {
    Seader* seader = context;
    if(seader->text_input) {
        text_input_reset(seader->text_input);
    }
}
