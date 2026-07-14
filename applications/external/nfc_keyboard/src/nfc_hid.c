#include "nfc_hid.h"

NfcHidApp* nfc_hid_alloc() {
    NfcHidApp* app = malloc(sizeof(NfcHidApp));

    app->view_port = view_port_alloc();

    // Open GUI and register view_port
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    // Save current USB Mode
    app->usb_mode_prev = furi_hal_usb_get_config();

    // Set USB mode to hid
    furi_hal_usb_set_config(&usb_hid, NULL);

    // Enable nfc
    app->nfc = nfc_alloc();
    app->scanner = nfc_scanner_alloc(app->nfc);
    app->device = nfc_device_alloc();

    // Set initial values
    app->running = true;
    app->detected = false;
    app->uid_str = furi_string_alloc();
    furi_string_set(app->uid_str, "N/A");

    // Register callbacks
    view_port_draw_callback_set(app->view_port, nfc_hid_render_callback, app);
    view_port_input_callback_set(app->view_port, nfc_hid_input_callback, app);
    nfc_scanner_start(app->scanner, nfc_hid_scanner_callback, app);

    return app;
}

void nfc_hid_free(NfcHidApp* app) {
    // Stop and free nfc
    nfc_device_free(app->device);
    nfc_scanner_stop(app->scanner);
    nfc_scanner_free(app->scanner);
    nfc_free(app->nfc);

    // Restore previous USB Mode
    furi_hal_usb_set_config(app->usb_mode_prev, NULL);

    // remove & free all stuff created by app
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_string_free(app->uid_str);
    free(app);
}

/** Entrypoint */
int32_t nfc_hid_app(void* p) {
    UNUSED(p);

    NfcHidApp* app = nfc_hid_alloc();

    while(app->running) {
        // Do something to receive callbacks
        furi_delay_ms(50);

        if(app->detected) {
            // Stop scanner to prevent multiple scans of same card
            nfc_scanner_stop(app->scanner);

            numlock();

            // Read uid
            size_t uid_len;
            const uint8_t* uid = nfc_device_get_uid(app->device, &uid_len);

            furi_string_set(app->uid_str, "");
            convertToHexString(app->uid_str, uid, uid_len);

            furi_delay_ms(50);
            write_string(furi_string_get_cstr(app->uid_str), furi_string_size(app->uid_str));

            nfc_device_clear(app->device);
            free((uint8_t*)uid);

            // Restart scanner with delay
            furi_delay_ms(2000);
            numlock();
            app->detected = false;
            nfc_scanner_start(app->scanner, nfc_hid_scanner_callback, app);
        }

        // Refresh UI
        view_port_update(app->view_port);
    }

    nfc_hid_free(app);

    return 0;
}
