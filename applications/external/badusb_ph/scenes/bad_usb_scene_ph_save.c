#include "../bad_usb_app_i.h"
#include <storage/storage.h>

typedef enum {
    PhSaveStateAsk = 0, // showing the "save?" question (widget)
    PhSaveStateName = 1, // showing the config-name text input
} PhSaveState;

#define PH_SAVE_EVENT_NAME_DONE 0x1000

static void
    bad_usb_scene_ph_save_widget_callback(GuiButtonType type, InputType input_type, void* context) {
    UNUSED(input_type);
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, type);
}

static void bad_usb_scene_ph_save_name_callback(void* context) {
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, PH_SAVE_EVENT_NAME_DONE);
}

static void bad_usb_scene_ph_save_show_question(BadUsbApp* app) {
    Widget* widget = app->widget;
    widget_reset(widget);
    widget_add_text_box_element(
        widget,
        0,
        0,
        128,
        64,
        AlignCenter,
        AlignTop,
        "\e#Save as config?\e#\nReuse these values\nfor this payload later",
        false);
    widget_add_button_element(
        widget, GuiButtonTypeLeft, "Run", bad_usb_scene_ph_save_widget_callback, app);
    widget_add_button_element(
        widget, GuiButtonTypeRight, "Save", bad_usb_scene_ph_save_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewWidget);
}

static void bad_usb_scene_ph_save_show_name(BadUsbApp* app) {
    TextInput* text_input = app->text_input;
    text_input_reset(text_input);
    strncpy(app->ph_config_name_buf, "config1", BAD_USB_PH_CONFIG_NAME_MAX);
    app->ph_config_name_buf[BAD_USB_PH_CONFIG_NAME_MAX - 1] = '\0';
    text_input_set_header_text(text_input, "Config name");
    text_input_set_result_callback(
        text_input,
        bad_usb_scene_ph_save_name_callback,
        app,
        app->ph_config_name_buf,
        BAD_USB_PH_CONFIG_NAME_MAX,
        false);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewTextInput);
}

void bad_usb_scene_ph_save_on_enter(void* context) {
    BadUsbApp* app = context;
    scene_manager_set_scene_state(app->scene_manager, BadUsbScenePhSave, PhSaveStateAsk);
    bad_usb_scene_ph_save_show_question(app);
}

bool bad_usb_scene_ph_save_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            // Run without saving
            scene_manager_next_scene(app->scene_manager, BadUsbSceneWork);
            consumed = true;
        } else if(event.event == GuiButtonTypeRight) {
            // Ask for a config name
            scene_manager_set_scene_state(app->scene_manager, BadUsbScenePhSave, PhSaveStateName);
            bad_usb_scene_ph_save_show_name(app);
            consumed = true;
        } else if(event.event == PH_SAVE_EVENT_NAME_DONE) {
            // Save (if a non-empty name was given) and run
            if(strlen(app->ph_config_name_buf) > 0) {
                Storage* storage = furi_record_open(RECORD_STORAGE);
                placeholder_config_save(
                    storage,
                    furi_string_get_cstr(app->payload_basename),
                    app->ph_config_name_buf,
                    &app->placeholders);
                furi_record_close(RECORD_STORAGE);
            }
            scene_manager_next_scene(app->scene_manager, BadUsbSceneWork);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        uint32_t state = scene_manager_get_scene_state(app->scene_manager, BadUsbScenePhSave);
        if(state == PhSaveStateName) {
            // Back out of naming -> return to the question
            scene_manager_set_scene_state(app->scene_manager, BadUsbScenePhSave, PhSaveStateAsk);
            bad_usb_scene_ph_save_show_question(app);
        } else {
            // Back out of the question -> return to config selection,
            // skipping the (now fully resolved) input scene.
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BadUsbScenePhConfig);
        }
        consumed = true;
    }
    return consumed;
}

void bad_usb_scene_ph_save_on_exit(void* context) {
    BadUsbApp* app = context;
    widget_reset(app->widget);
    text_input_reset(app->text_input);
}
