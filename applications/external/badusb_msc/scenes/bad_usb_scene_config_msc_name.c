#include "../bad_usb_app_i.h"

enum TextInputResult {
    TextInputResultOk,
};

static void bad_usb_scene_config_msc_name_text_input_callback(void* context) {
    BadUsbApp* bad_usb = context;
    view_dispatcher_send_custom_event(bad_usb->view_dispatcher, TextInputResultOk);
}

void bad_usb_scene_config_msc_name_on_enter(void* context) {
    BadUsbApp* bad_usb = context;
    TextInput* text_input = bad_usb->text_input;

    strlcpy(bad_usb->msc_name_buf, bad_usb->msc_name, sizeof(bad_usb->msc_name_buf));
    text_input_set_header_text(text_input, "Set storage name (max 11)");

    text_input_set_result_callback(
        text_input,
        bad_usb_scene_config_msc_name_text_input_callback,
        bad_usb,
        bad_usb->msc_name_buf,
        sizeof(bad_usb->msc_name_buf),
        true);

    view_dispatcher_switch_to_view(bad_usb->view_dispatcher, BadUsbAppViewTextInput);
}

bool bad_usb_scene_config_msc_name_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* bad_usb = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == TextInputResultOk) {
            if(bad_usb->msc_name_buf[0] != '\0') {
                strlcpy(bad_usb->msc_name, bad_usb->msc_name_buf, sizeof(bad_usb->msc_name));
            } else {
                strlcpy(bad_usb->msc_name, BAD_USB_MSC_DEFAULT_NAME, sizeof(bad_usb->msc_name));
            }
            bad_usb_save_settings(bad_usb);
            Storage* storage = furi_record_open(RECORD_STORAGE);
            combo_fat16_set_label(storage, COMBO_IMAGE_PATH, bad_usb->msc_name);
            furi_record_close(RECORD_STORAGE);
        }
        scene_manager_previous_scene(bad_usb->scene_manager);
    }
    return consumed;
}

void bad_usb_scene_config_msc_name_on_exit(void* context) {
    BadUsbApp* bad_usb = context;
    text_input_reset(bad_usb->text_input);
}
