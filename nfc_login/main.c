#include "nfc_login_app.h"
#include "crypto/nfc_login_crypto.h"
#include "crypto/nfc_login_passcode.h"
#include "settings/nfc_login_settings.h"
#include "storage/nfc_login_card_storage.h"
#include "scan/nfc_login_scan.h"
#include "hid/nfc_login_hid.h"
#include "scenes/scene_manager.h"
#include "scenes/settings/passcode_canvas.h"
#include "cli/nfc_login_cli.h"

int32_t nfc_login(void* p) {
    UNUSED(p);
    
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    
    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    
    scene_manager_init(app);
    app_load_settings(app);
    
    if(app->hid_mode == HidModeBle) {
        app_start_ble_advertising();
    }
    
    ensure_crypto_key();
    
    app->passcode_prompt_active = false;
    app->passcode_sequence_len = 0;
    app->passcode_needed = false;
    app->passcode_failed_attempts = 0;
    memset(app->passcode_sequence, 0, sizeof(app->passcode_sequence));
    
    if(get_passcode_disabled()) {
        app_load_cards(app);
        app_switch_to_view(app, ViewSubmenu);
    } else {
        bool has_passcode_set = has_passcode();
        app->passcode_prompt_active = true;
        app->passcode_sequence_len = 0;
        app->widget_state = has_passcode_set ? 7 : 6;
        app_switch_to_view(app, ViewPasscodeCanvas);
    }
    
    nfc_login_cli_register_commands(app);
    nfc_login_cli_set_app_instance(app);
    
    view_dispatcher_run(app->view_dispatcher);
    
    nfc_login_cli_unregister_commands();
    nfc_login_cli_clear_app_instance();
    if(app->scanning) {
        app->scanning = false;
        if(app->scan_thread) {
            furi_thread_join(app->scan_thread);
            furi_thread_free(app->scan_thread);
        }
    }
    if(app->enrollment_scanning) {
        app->enrollment_scanning = false;
        if(app->enroll_scan_thread) {
            furi_thread_join(app->enroll_scan_thread);
            furi_thread_free(app->enroll_scan_thread);
        }
    }
    
    // CRITICAL: Only deinitialize USB if USB mode was used
    // BLE mode should NEVER touch USB functions
    if(app->hid_mode == HidModeUsb && app->previous_usb_config) {
        deinitialize_hid_with_restore_and_mode(app->previous_usb_config, HidModeUsb);
    } else if(app->hid_mode == HidModeBle) {
        // BLE mode - stop advertising but don't deinit (let it stay connected)
        app_stop_ble_advertising();
    }
    
    view_dispatcher_remove_view(app->view_dispatcher, ViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, ViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, ViewFileBrowser);
    view_dispatcher_remove_view(app->view_dispatcher, ViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, ViewPasscodeCanvas);
    
    submenu_free(app->submenu);
    text_input_free(app->text_input);
    widget_free(app->widget);
    file_browser_free(app->file_browser);
    furi_string_free(app->fb_output_path);
    byte_input_free(app->byte_input);
    passcode_canvas_view_free(app->passcode_canvas_view);
    view_dispatcher_free(app->view_dispatcher);
    
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    
    free(app);
    
    return 0;
}