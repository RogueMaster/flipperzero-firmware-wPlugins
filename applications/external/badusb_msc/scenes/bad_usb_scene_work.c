#include "../helpers/ducky_script.h"
#include "../bad_usb_app_i.h"
#include "../views/bad_usb_view.h"
#include <furi_hal.h>
#include "toolbox/path.h"

// Confirm safe ejection before disconnecting the virtual disk to avoid cached Windows writes.
static bool combo_confirm_safe_eject(BadUsbApp* app) {
    DialogMessage* msg = dialog_message_alloc();
    dialog_message_set_header(msg, "Eject BADUSBMSC", 64, 7, AlignCenter, AlignTop);
    dialog_message_set_text(msg, "Ejected in Windows?", 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(msg, "No", NULL, "Yes");
    DialogMessageButton choice = dialog_message_show(app->dialogs, msg);
    dialog_message_free(msg);
    return choice == DialogMessageButtonRight;
}

void bad_usb_scene_work_button_callback(InputKey key, void* context) {
    furi_assert(context);
    BadUsbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, key);
}

bool bad_usb_scene_work_on_event(void* context, SceneManagerEvent event) {
    BadUsbApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == InputKeyLeft) {
            if(bad_usb_view_is_idle_state(app->bad_usb_view) && combo_confirm_safe_eject(app)) {
                bad_usb_script_close(app->bad_usb_script);
                app->bad_usb_script = NULL;

                scene_manager_set_scene_state(app->scene_manager, BadUsbSceneConfig, 0);
                scene_manager_next_scene(app->scene_manager, BadUsbSceneConfig);
            }
            consumed = true;
        } else if(event.event == InputKeyOk) {
            bad_usb_script_start_stop(app->bad_usb_script);
            consumed = true;
        } else if(event.event == InputKeyRight) {
            // Restore the original BadUSB USB/BLE toggle while idle.
            // The MSC disk still uses USB; BLE carries only the HID reports.
            if(bad_usb_view_is_idle_state(app->bad_usb_view)) {
                bad_usb_set_interface(
                    app,
                    app->interface == BadUsbHidInterfaceBle ? BadUsbHidInterfaceUsb :
                                                              BadUsbHidInterfaceBle);
                bad_usb_script_close(app->bad_usb_script);
                app->bad_usb_script = bad_usb_script_open(
                    app->file_path, &app->interface, &app->script_hid_cfg, false);
                if(!bad_usb_script_set_keyboard_layout(app->bad_usb_script, app->keyboard_layout)) {
                    bad_usb_files_message(app, "Keyboard error", "Selected .kl not loaded");
                    scene_manager_search_and_switch_to_previous_scene(
                        app->scene_manager, BadUsbSceneHome);
                    return true;
                }
                bad_usb_save_settings(app);
            } else {
                bad_usb_script_pause_resume(app->bad_usb_script);
            }
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Block navigation until the user confirms that the volume was ejected.
        if(!bad_usb_view_is_idle_state(app->bad_usb_view)) return true;
        if(combo_confirm_safe_eject(app)) {
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    } else if(event.type == SceneManagerEventTypeTick) {
        bad_usb_view_set_state(app->bad_usb_view, bad_usb_script_get_state(app->bad_usb_script));
        bad_usb_view_set_interface(app->bad_usb_view, app->interface);
    }
    return consumed;
}

void bad_usb_scene_work_on_enter(void* context) {
    BadUsbApp* app = context;
    if(app->storage_prepare_thread) {
        if(furi_thread_get_state(app->storage_prepare_thread) != FuriThreadStateStopped) {
            bad_usb_files_message(app, "Preparing storage", "Please retry when ready");
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, BadUsbSceneHome);
            return;
        }
        furi_thread_join(app->storage_prepare_thread);
        app->storage_prepare_result = furi_thread_get_return_code(app->storage_prepare_thread);
        app->storage_prepared = app->storage_prepare_result == ComboFat16PrepareReady;
        furi_thread_free(app->storage_prepare_thread);
        app->storage_prepare_thread = NULL;
    }
    if(!app->storage_prepared) {
        if(app->storage_prepare_result == ComboFat16PrepareInvalidImage) {
            bad_usb_files_message(app, "Invalid drive.img", "Backup damaged image");
        } else {
            bad_usb_files_message(app, "Storage unavailable", "Check SD / free space");
        }
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, BadUsbSceneHome);
        return;
    }
    // Opening script the first time:
    // - copy user settings as base config
    // - load ID/BLE_ID/BT_ID config if present
    // Then disable this until next script selected so user can customize options
    bool first_script_load = scene_manager_get_scene_state(app->scene_manager, BadUsbSceneWork);
    if(first_script_load) {
        memcpy(&app->script_hid_cfg, &app->user_hid_cfg, sizeof(app->script_hid_cfg));
        // Official BadUSB reads ID on the script's first line. The composite
        // USB descriptor must already know this identity before enumeration.
        if(bad_usb_script_preload_usb_identity(app->file_path, &app->script_hid_cfg)) {
            app->interface = BadUsbHidInterfaceUsb;
        }
        scene_manager_set_scene_state(app->scene_manager, BadUsbSceneWork, false);
    }
    bad_usb_hid_get_interface(BadUsbHidInterfaceUsb)->adjust_config(&app->script_hid_cfg);
    // Start MSC only after the effective script/config USB identity is known.
    // Never recreate drive.img here; ComboRuntime uses the existing image.
    if(!app->combo_runtime) app->combo_runtime = combo_runtime_start(&app->script_hid_cfg.usb);
    if(!app->combo_runtime) {
        bad_usb_files_message(app, "Storage unavailable", "Check SD / disk image");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, BadUsbSceneHome);
        return;
    }
    bad_usb_view_set_interface(app->bad_usb_view, app->interface);
    // Interface and config are passed as pointers as ID/BLE_ID/BT_ID config can modify them
    app->bad_usb_script = bad_usb_script_open(
        app->file_path, &app->interface, &app->script_hid_cfg, first_script_load);
    if(!bad_usb_script_set_keyboard_layout(app->bad_usb_script, app->keyboard_layout)) {
        bad_usb_files_message(app, "Keyboard error", "Selected .kl not loaded");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, BadUsbSceneHome);
        return;
    }

    FuriString* file_name;
    file_name = furi_string_alloc();
    path_extract_filename(app->file_path, file_name, true);
    bad_usb_view_set_file_name(app->bad_usb_view, furi_string_get_cstr(file_name));
    furi_string_free(file_name);

    FuriString* layout;
    layout = furi_string_alloc();
    path_extract_filename(app->keyboard_layout, layout, true);
    bad_usb_view_set_layout(app->bad_usb_view, furi_string_get_cstr(layout));
    furi_string_free(layout);

    bad_usb_view_set_state(app->bad_usb_view, bad_usb_script_get_state(app->bad_usb_script));

    bad_usb_view_set_button_callback(app->bad_usb_view, bad_usb_scene_work_button_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, BadUsbAppViewWork);
}

void bad_usb_scene_work_on_exit(void* context) {
    BadUsbApp* app = context;
    // BadUSB worker must stop before the USB interface is restored.
    if(app->bad_usb_script) {
        bad_usb_script_close(app->bad_usb_script);
        app->bad_usb_script = NULL;
    }
    if(app->combo_runtime) {
        combo_runtime_stop(app->combo_runtime);
        app->combo_runtime = NULL;
    }
}
