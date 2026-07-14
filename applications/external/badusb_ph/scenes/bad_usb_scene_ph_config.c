#include "../bad_usb_app_i.h"
#include <storage/storage.h>

// Submenu items:
//   0 .. ph_config_count-1  -> saved configs (by name)
//   ph_config_count         -> "Manual entry"

static void bad_usb_scene_ph_config_submenu_callback(void* context, uint32_t index) {
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bad_usb_scene_ph_config_on_enter(void* context) {
    BadUsbApp* app = context;
    Submenu* submenu = app->submenu;
    submenu_reset(submenu);

    snprintf(
        app->ph_header_buf,
        sizeof(app->ph_header_buf),
        "%zu placeholder%s - pick config",
        app->placeholders.count,
        app->placeholders.count == 1 ? "" : "s");
    submenu_set_header(submenu, app->ph_header_buf);

    // Refresh the list of saved configs for this payload
    for(size_t i = 0; i < app->ph_config_count; i++) {
        furi_string_free(app->ph_config_names[i]);
    }
    app->ph_config_count = 0;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    app->ph_config_count = placeholder_config_list(
        storage,
        furi_string_get_cstr(app->payload_basename),
        app->ph_config_names,
        BAD_USB_PH_CONFIG_LIST_MAX);
    furi_record_close(RECORD_STORAGE);

    for(size_t i = 0; i < app->ph_config_count; i++) {
        submenu_add_item(
            submenu,
            furi_string_get_cstr(app->ph_config_names[i]),
            i,
            bad_usb_scene_ph_config_submenu_callback,
            app);
    }
    submenu_add_item(
        submenu,
        "Manual entry",
        app->ph_config_count,
        bad_usb_scene_ph_config_submenu_callback,
        app);

    submenu_set_selected_item(submenu, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewPhConfig);
}

bool bad_usb_scene_ph_config_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        uint32_t index = event.event;

        // Start from a clean slate every time a choice is made
        placeholder_map_clear_values(&app->placeholders);

        if(index < app->ph_config_count) {
            // Load the chosen config; placeholders not present in it stay
            // unresolved and will be asked for in the input scene.
            Storage* storage = furi_record_open(RECORD_STORAGE);
            placeholder_config_load(
                storage,
                furi_string_get_cstr(app->payload_basename),
                furi_string_get_cstr(app->ph_config_names[index]),
                &app->placeholders);
            furi_record_close(RECORD_STORAGE);
        }
        // else: "Manual entry" -> everything stays unresolved

        app->ph_input_idx = 0;
        if(placeholder_map_unresolved_count(&app->placeholders) == 0) {
            // Fully resolved from a saved config: run straight away
            scene_manager_next_scene(app->scene_manager, BadUsbSceneWork);
        } else {
            scene_manager_next_scene(app->scene_manager, BadUsbScenePhInput);
        }
    }
    return consumed;
}

void bad_usb_scene_ph_config_on_exit(void* context) {
    BadUsbApp* app = context;
    submenu_reset(app->submenu);
}
