#include "../bad_usb_app_i.h"

static void bad_usb_scene_ph_input_callback(void* context) {
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

// Configure the text input for the next unresolved placeholder. If every
// placeholder is resolved, advance to the save scene instead.
static void bad_usb_scene_ph_input_setup(BadUsbApp* app) {
    size_t idx = app->placeholders.count;
    for(size_t i = 0; i < app->placeholders.count; i++) {
        if(!app->placeholders.entries[i].resolved) {
            idx = i;
            break;
        }
    }

    if(idx >= app->placeholders.count) {
        // All placeholders filled -> offer to save, then run
        scene_manager_next_scene(app->scene_manager, BadUsbScenePhSave);
        return;
    }

    app->ph_input_idx = idx;
    snprintf(
        app->ph_header_buf,
        sizeof(app->ph_header_buf),
        "Value for [%s]",
        furi_string_get_cstr(app->placeholders.entries[idx].name));
    app->ph_text_buf[0] = '\0';

    TextInput* text_input = app->text_input;
    text_input_reset(text_input);
    text_input_set_header_text(text_input, app->ph_header_buf);
    text_input_set_result_callback(
        text_input,
        bad_usb_scene_ph_input_callback,
        app,
        app->ph_text_buf,
        PLACEHOLDER_VALUE_MAX,
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewTextInput);
}

void bad_usb_scene_ph_input_on_enter(void* context) {
    bad_usb_scene_ph_input_setup(context);
}

bool bad_usb_scene_ph_input_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        // Store the value just entered for the current placeholder...
        placeholder_set_value(&app->placeholders, app->ph_input_idx, app->ph_text_buf);
        // ...then prompt for the next one (or move on to the save scene).
        bad_usb_scene_ph_input_setup(app);
    }
    return consumed;
}

void bad_usb_scene_ph_input_on_exit(void* context) {
    BadUsbApp* app = context;
    text_input_reset(app->text_input);
}
