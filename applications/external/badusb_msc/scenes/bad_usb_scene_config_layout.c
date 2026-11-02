#include "../bad_usb_app_i.h"
#include <storage/storage.h>

static void bad_usb_layout_select(BadUsbApp* app) {
    FuriString* candidate = furi_string_alloc();
    furi_string_set(candidate, app->keyboard_layout);
    FuriString* browser_path = furi_string_alloc();
    if(!furi_string_empty(candidate)) {
        furi_string_set(browser_path, candidate);
    } else {
        furi_string_set(browser_path, BAD_USB_APP_PATH_LAYOUT_FOLDER);
    }

    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(
        &options, BAD_USB_APP_LAYOUT_EXTENSION, &I_keyboard_10px);
    options.base_path = BAD_USB_APP_PATH_LAYOUT_FOLDER;
    options.skip_assets = false;

    bool selected = dialog_file_browser_show(app->dialogs, candidate, browser_path, &options);
    if(selected) {
        uint16_t test_layout[128];
        if(bad_usb_keyboard_layout_read(candidate, test_layout)) {
            // Do not update what Options shows unless the same reader used by
            // the interpreter has successfully loaded the selected mapping.
            if(!app->bad_usb_script ||
               bad_usb_script_set_keyboard_layout(app->bad_usb_script, candidate)) {
                furi_string_set(app->keyboard_layout, candidate);
                bad_usb_save_settings(app);
            } else {
                bad_usb_files_message(app, "Layout error", "Cannot apply while running");
            }
        } else {
            bad_usb_files_message(app, "Layout error", "Cannot read 256-byte .kl");
        }
    }
    furi_string_free(candidate);
    furi_string_free(browser_path);
}

void bad_usb_scene_config_layout_on_enter(void* context) {
    BadUsbApp* app = context;
    bad_usb_layout_select(app);
    scene_manager_previous_scene(app->scene_manager);
}

bool bad_usb_scene_config_layout_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void bad_usb_scene_config_layout_on_exit(void* context) {
    UNUSED(context);
}
