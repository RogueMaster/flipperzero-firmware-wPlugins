#include "networking_nfc_qr_app.h"
#include <furi.h>
#include <gui/gui.h>
#include <notification/notification_messages.h>

// Helper to save QR code file
static bool save_qr_file(Storage* storage, const char* file_path, const char* url) {
    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool success = false;
    do {
        if (!flipper_format_file_open_always(file, file_path)) break;
        if (!flipper_format_write_header_cstr(file, "QRCode", 0)) break;
        if (!flipper_format_write_string_cstr(file, "Message", url)) break;
        success = true;
    } while (false);
    flipper_format_free(file);
    return success;
}

// Helper to save NFC NTAG215 file
static bool save_nfc_file(Storage* storage, const char* file_path, const char* url) {
    uint8_t uid[7];
    furi_hal_random_fill_buf(uid, sizeof(uid));
    uid[0] = 0x04; // NXP manufacturer prefix for NTAG

    uint8_t bcc0 = 0x88 ^ uid[0] ^ uid[1] ^ uid[2];
    uint8_t bcc1 = uid[3] ^ uid[4] ^ uid[5] ^ uid[6];

    uint8_t pages[135][4];
    memset(pages, 0, sizeof(pages));

    // Page 0: UID0..2, BCC0
    pages[0][0] = uid[0];
    pages[0][1] = uid[1];
    pages[0][2] = uid[2];
    pages[0][3] = bcc0;

    // Page 1: UID3..6
    pages[1][0] = uid[3];
    pages[1][1] = uid[4];
    pages[1][2] = uid[5];
    pages[1][3] = uid[6];

    // Page 2: BCC1, Internal byte, Static lock bytes
    pages[2][0] = bcc1;
    pages[2][1] = 0x48;
    pages[2][2] = 0x00;
    pages[2][3] = 0x00;

    // Page 3: CC (Capability Container for NTAG215: 496 user bytes, read/write)
    pages[3][0] = 0xE1;
    pages[3][1] = 0x10;
    pages[3][2] = 0x3E;
    pages[3][3] = 0x00;

    // NDEF URI Prefix detection
    uint8_t prefix_code = 0x00;
    const char* url_body = url;
    if (strncmp(url, "https://www.", 12) == 0) {
        prefix_code = 0x02;
        url_body = url + 12;
    } else if (strncmp(url, "http://www.", 11) == 0) {
        prefix_code = 0x01;
        url_body = url + 11;
    } else if (strncmp(url, "https://", 8) == 0) {
        prefix_code = 0x04;
        url_body = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        prefix_code = 0x03;
        url_body = url + 7;
    } else if (strncmp(url, "tel:", 4) == 0) {
        prefix_code = 0x05;
        url_body = url + 4;
    } else if (strncmp(url, "mailto:", 7) == 0) {
        prefix_code = 0x06;
        url_body = url + 7;
    }

    size_t url_body_len = strlen(url_body);
    uint8_t payload_len = (uint8_t)(1 + url_body_len);
    uint8_t ndef_msg_len = (uint8_t)(4 + payload_len);

    uint8_t tlv_buf[512];
    size_t tlv_len = 0;
    tlv_buf[tlv_len++] = 0x03;          // NDEF Message TLV tag
    tlv_buf[tlv_len++] = ndef_msg_len;   // Length
    tlv_buf[tlv_len++] = 0xD1;          // Record header: MB=1, ME=1, SR=1, TNF=001
    tlv_buf[tlv_len++] = 0x01;          // Type length = 1
    tlv_buf[tlv_len++] = payload_len;   // Payload length
    tlv_buf[tlv_len++] = 0x55;          // Type: 'U' (URI)
    tlv_buf[tlv_len++] = prefix_code;   // URI Prefix
    memcpy(&tlv_buf[tlv_len], url_body, url_body_len);
    tlv_len += url_body_len;
    tlv_buf[tlv_len++] = 0xFE;          // Terminator TLV

    // Write TLV into pages starting at Page 4
    for (size_t i = 0; i < tlv_len && (4 + i / 4) < 130; i++) {
        pages[4 + (i / 4)][i % 4] = tlv_buf[i];
    }

    // Trailer pages for NTAG215
    pages[130][0] = 0x00;
    pages[130][1] = 0x00;
    pages[130][2] = 0x00;
    pages[130][3] = 0xBD;

    pages[131][0] = 0x04;
    pages[131][1] = 0x00;
    pages[131][2] = 0x00;
    pages[131][3] = 0xFF;

    pages[132][0] = 0x00;
    pages[132][1] = 0x05;
    pages[132][2] = 0x00;
    pages[132][3] = 0x00;

    pages[133][0] = 0xFF;
    pages[133][1] = 0xFF;
    pages[133][2] = 0xFF;
    pages[133][3] = 0xFF;

    pages[134][0] = 0x00;
    pages[134][1] = 0x00;
    pages[134][2] = 0x00;
    pages[134][3] = 0x00;

    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool success = false;
    do {
        if (!flipper_format_file_open_always(file, file_path)) break;
        if (!flipper_format_write_header_cstr(file, "Flipper NFC device", 4)) break;
        if (!flipper_format_write_comment_cstr(
                file,
                "Device type can be ISO14443-3A, ISO14443-3B, ISO14443-4A, ISO14443-4B, ISO15693-3, FeliCa, NTAG/Ultralight, Mifare Classic, Mifare Plus, Mifare DESFire, SLIX, ST25TB, EMV")) break;
        if (!flipper_format_write_string_cstr(file, "Device type", "NTAG/Ultralight")) break;
        if (!flipper_format_write_comment_cstr(file, "UID is common for all formats")) break;
        if (!flipper_format_write_hex(file, "UID", uid, 7)) break;
        if (!flipper_format_write_comment_cstr(file, "ISO14443-3A specific data")) break;
        uint8_t atqa[] = {0x00, 0x44};
        if (!flipper_format_write_hex(file, "ATQA", atqa, 2)) break;
        uint8_t sak = 0x00;
        if (!flipper_format_write_hex(file, "SAK", &sak, 1)) break;
        if (!flipper_format_write_comment_cstr(file, "NTAG/Ultralight specific data")) break;
        uint32_t data_format_ver = 2;
        if (!flipper_format_write_uint32(file, "Data format version", &data_format_ver, 1)) break;
        if (!flipper_format_write_string_cstr(file, "NTAG/Ultralight type", "NTAG215")) break;
        uint8_t signature[32] = {0};
        if (!flipper_format_write_hex(file, "Signature", signature, 32)) break;
        uint8_t mifare_ver[] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x11, 0x03};
        if (!flipper_format_write_hex(file, "Mifare version", mifare_ver, 8)) break;
        uint32_t counter = 0;
        uint8_t tearing = 0;
        if (!flipper_format_write_uint32(file, "Counter 0", &counter, 1)) break;
        if (!flipper_format_write_hex(file, "Tearing 0", &tearing, 1)) break;
        if (!flipper_format_write_uint32(file, "Counter 1", &counter, 1)) break;
        if (!flipper_format_write_hex(file, "Tearing 1", &tearing, 1)) break;
        if (!flipper_format_write_uint32(file, "Counter 2", &counter, 1)) break;
        if (!flipper_format_write_hex(file, "Tearing 2", &tearing, 1)) break;
        uint32_t pages_total = 135;
        if (!flipper_format_write_uint32(file, "Pages total", &pages_total, 1)) break;
        if (!flipper_format_write_uint32(file, "Pages read", &pages_total, 1)) break;

        char page_key[16];
        bool page_error = false;
        for (uint16_t i = 0; i < 135; i++) {
            snprintf(page_key, sizeof(page_key), "Page %u", i);
            if (!flipper_format_write_hex(file, page_key, pages[i], 4)) {
                page_error = true;
                break;
            }
        }
        if (page_error) break;

        uint32_t failed_auth = 0;
        if (!flipper_format_write_uint32(file, "Failed authentication attempts", &failed_auth, 1)) break;

        success = true;
    } while (false);
    flipper_format_free(file);
    return success;
}

// Helper to load QR code
static bool load_qr_code(NfcQrApp* app) {
    if (app->qrcode) {
        free(app->qrcode->modules);
        free(app->qrcode);
        app->qrcode = NULL;
    }

    if (furi_string_empty(app->qr_file_path)) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    bool result = false;

    do {
        if (!flipper_format_file_open_existing(file, furi_string_get_cstr(app->qr_file_path))) {
            break;
        }

        if (!flipper_format_read_string(file, "Message", temp_str)) {
            break;
        }

        app->qrcode = malloc(sizeof(QRCode));
        memset(app->qrcode, 0, sizeof(QRCode));

        bool initialized = false;
        // Try version 3 to 6 (Medium ECC first, then Low ECC)
        for (uint8_t ver = 3; ver <= 6 && !initialized; ver++) {
            uint8_t* modules = malloc(qrcode_getBufferSize(ver));
            if (qrcode_initBytes(
                    app->qrcode,
                    modules,
                    MODE_BYTE,
                    ver,
                    ECC_MEDIUM,
                    (uint8_t*)furi_string_get_cstr(temp_str),
                    furi_string_size(temp_str)) == 0) {
                app->qrcode->modules = modules;
                initialized = true;
                break;
            }
            if (qrcode_initBytes(
                    app->qrcode,
                    modules,
                    MODE_BYTE,
                    ver,
                    ECC_LOW,
                    (uint8_t*)furi_string_get_cstr(temp_str),
                    furi_string_size(temp_str)) == 0) {
                app->qrcode->modules = modules;
                initialized = true;
                break;
            }
            free(modules);
        }

        if (!initialized) {
            free(app->qrcode);
            app->qrcode = NULL;
            break;
        }

        result = true;

        // Update model with new QR code
        if (app->emulate_view) {
            with_view_model(
                app->emulate_view,
                NfcQrAppModel * model,
                {
                    model->qrcode = app->qrcode;
                },
                true);
        }
    } while (false);

    furi_string_free(temp_str);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    return result;
}

// Emulation View Draw Callback
static void emulate_view_draw_callback(Canvas* canvas, void* model_ptr) {
    NfcQrAppModel* model = model_ptr;
    canvas_clear(canvas);

    if (model->qrcode) {
        int size = model->qrcode->size;
        int pixel_size = 64 / size;
        if (pixel_size < 1) pixel_size = 1;
        int offset_x = (128 - size * pixel_size) / 2;
        int offset_y = (64 - size * pixel_size) / 2;

        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                if (qrcode_getModule(model->qrcode, x, y)) {
                    canvas_draw_box(canvas, offset_x + x * pixel_size, offset_y + y * pixel_size, pixel_size, pixel_size);
                }
            }
        }
    } else {
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "No QR Code");
    }
}

// Submenu Callback
static void submenu_callback(void* context, uint32_t index) {
    ViewDispatcher* view_dispatcher = context;
    view_dispatcher_send_custom_event(view_dispatcher, index);
}

// Emulation View Input Callback
static bool emulate_view_input_callback(InputEvent* event, void* context) {
    NfcQrApp* app = context;
    if (event->type == InputTypeShort && event->key == InputKeyBack) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventEmulateBack);
        return true;
    }
    return false;
}

// Result Timer Callback
static void result_timer_callback(void* context) {
    NfcQrApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventDismissResult);
}

// Widget Button Callback (for OK)
static void widget_button_callback(GuiButtonType result, InputType type, void* context) {
    NfcQrApp* app = context;
    if (result == GuiButtonTypeCenter && type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventDismissResult);
    }
}

// Text Input Done Callback
static void text_input_done_callback(void* context) {
    NfcQrApp* app = context;
    if (app->text_input_mode == NfcQrTextInputModeName) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventNameDone);
    } else if (app->text_input_mode == NfcQrTextInputModeUrl) {
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventUrlDone);
    }
}

// Custom Event Callback
static bool custom_event_callback(void* context, uint32_t event) {
    NfcQrApp* app = context;

    switch (event) {
        case NfcQrCustomEventMenuSelectQr: {
            DialogsFileBrowserOptions browser_options;
            dialog_file_browser_set_basic_options(&browser_options, ".qrcode", NULL);
            dialog_file_browser_show(app->dialogs, app->qr_file_path, app->qr_file_path, &browser_options);
            load_qr_code(app);
            return true;
        }
        case NfcQrCustomEventMenuSelectNfc: {
            DialogsFileBrowserOptions browser_options;
            dialog_file_browser_set_basic_options(&browser_options, ".nfc", NULL);
            dialog_file_browser_show(app->dialogs, app->nfc_file_path, app->nfc_file_path, &browser_options);
            return true;
        }
        case NfcQrCustomEventMenuCreate: {
            app->text_input_mode = NfcQrTextInputModeName;
            strncpy(app->name_store, "MyCard", sizeof(app->name_store));
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "File Name (without ext)");
            text_input_set_result_callback(
                app->text_input,
                text_input_done_callback,
                app,
                app->name_store,
                sizeof(app->name_store),
                true);
            app->current_view = NfcQrAppViewTextInput;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewTextInput);
            return true;
        }
        case NfcQrCustomEventNameDone: {
            // Strip .nfc or .qrcode extension if user explicitly typed it
            char* dot = strrchr(app->name_store, '.');
            if (dot && (strcasecmp(dot, ".nfc") == 0 || strcasecmp(dot, ".qrcode") == 0)) {
                *dot = '\0';
            }
            if (strlen(app->name_store) == 0) {
                strncpy(app->name_store, "Card", sizeof(app->name_store));
            }

            app->text_input_mode = NfcQrTextInputModeUrl;
            strncpy(app->text_store, "https://", sizeof(app->text_store));
            text_input_reset(app->text_input);
            text_input_set_header_text(app->text_input, "Enter URL");
            text_input_set_result_callback(
                app->text_input,
                text_input_done_callback,
                app,
                app->text_store,
                sizeof(app->text_store),
                false);
            app->current_view = NfcQrAppViewTextInput;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewTextInput);
            return true;
        }
        case NfcQrCustomEventUrlDone: {
            if (strlen(app->text_store) == 0 || strcmp(app->text_store, "https://") == 0 ||
                strcmp(app->text_store, "http://") == 0) {
                widget_reset(app->widget);
                widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "Error!");
                widget_add_string_element(
                    app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "URL cannot be empty");
                widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", widget_button_callback, app);
                furi_timer_start(app->popup_timer, furi_ms_to_ticks(2500));
                app->current_view = NfcQrAppViewWidget;
                view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewWidget);
                return true;
            }

            furi_string_printf(app->qr_file_path, "/ext/apps_data/qrcodes/%s.qrcode", app->name_store);
            furi_string_printf(app->nfc_file_path, "/ext/nfc/%s.nfc", app->name_store);

            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_simply_mkdir(storage, "/ext/apps_data");
            storage_simply_mkdir(storage, "/ext/apps_data/qrcodes");
            storage_simply_mkdir(storage, "/ext/nfc");

            bool qr_ok = save_qr_file(storage, furi_string_get_cstr(app->qr_file_path), app->text_store);
            bool nfc_ok = save_nfc_file(storage, furi_string_get_cstr(app->nfc_file_path), app->text_store);
            furi_record_close(RECORD_STORAGE);

            if (qr_ok && nfc_ok) {
                load_qr_code(app);
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Created Successfully!");
                char msg[192];
                snprintf(
                    msg,
                    sizeof(msg),
                    "%s.qrcode\n%s.nfc\nReady to emulate!",
                    app->name_store,
                    app->name_store);
                widget_add_string_multiline_element(app->widget, 64, 18, AlignCenter, AlignTop, FontSecondary, msg);
                widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", widget_button_callback, app);
                furi_timer_start(app->popup_timer, furi_ms_to_ticks(2500));
            } else {
                widget_reset(app->widget);
                widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "Save Error!");
                widget_add_string_element(
                    app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Could not write files");
                widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", widget_button_callback, app);
                furi_timer_start(app->popup_timer, furi_ms_to_ticks(2500));
            }
            app->current_view = NfcQrAppViewWidget;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewWidget);
            return true;
        }
        case NfcQrCustomEventDismissResult: {
            if (app->popup_timer) {
                furi_timer_stop(app->popup_timer);
            }
            if (app->current_view == NfcQrAppViewWidget) {
                app->current_view = NfcQrAppViewSubmenu;
                view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewSubmenu);
            }
            return true;
        }
        case NfcQrCustomEventMenuStart: {
            bool qr_ok = !furi_string_empty(app->qr_file_path) &&
                         furi_string_end_with(app->qr_file_path, ".qrcode");
            bool nfc_ok = !furi_string_empty(app->nfc_file_path) &&
                          furi_string_end_with(app->nfc_file_path, ".nfc");

            if (!qr_ok || !nfc_ok) {
                widget_reset(app->widget);
                widget_add_string_element(
                    app->widget, 64, 4, AlignCenter, AlignTop, FontPrimary, "Missing Selection!");
                if (!qr_ok && !nfc_ok) {
                    widget_add_string_multiline_element(
                        app->widget,
                        64,
                        18,
                        AlignCenter,
                        AlignTop,
                        FontSecondary,
                        "Select both:\n- QR Code (.qrcode)\n- NFC File (.nfc)");
                } else if (!qr_ok) {
                    widget_add_string_multiline_element(
                        app->widget,
                        64,
                        20,
                        AlignCenter,
                        AlignTop,
                        FontSecondary,
                        "Missing QR Code!\nSelect a .qrcode file");
                } else {
                    widget_add_string_multiline_element(
                        app->widget,
                        64,
                        20,
                        AlignCenter,
                        AlignTop,
                        FontSecondary,
                        "Missing NFC File!\nSelect a .nfc file");
                }
                widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", widget_button_callback, app);
                furi_timer_start(app->popup_timer, furi_ms_to_ticks(2500));
                app->current_view = NfcQrAppViewWidget;
                view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewWidget);
                return true;
            }

            // Both files are valid and selected - proceed to emulation
            app->current_view = NfcQrAppViewEmulate;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewEmulate);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventLoadFile);
            return true;
        }

        case NfcQrCustomEventLoadFile: {
            if (nfc_device_load(app->nfc_device, furi_string_get_cstr(app->nfc_file_path))) {
                view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventAllocNfc);
            } else {
                widget_reset(app->widget);
                widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, "NFC Load Error!");
                widget_add_string_element(
                    app->widget, 64, 25, AlignCenter, AlignTop, FontSecondary, "Could not read NFC file");
                widget_add_button_element(app->widget, GuiButtonTypeCenter, "OK", widget_button_callback, app);
                furi_timer_start(app->popup_timer, furi_ms_to_ticks(2500));
                app->current_view = NfcQrAppViewWidget;
                view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewWidget);
            }
            return true;
        }

        case NfcQrCustomEventAllocNfc: {
            NfcProtocol protocol = nfc_device_get_protocol(app->nfc_device);

            if (protocol == NfcProtocolInvalid) {
                return true;
            }

            const NfcDeviceData* data = nfc_device_get_data(app->nfc_device, protocol);

            if (data) {
                if (app->nfc_listener) {
                    nfc_listener_stop(app->nfc_listener);
                    nfc_listener_free(app->nfc_listener);
                }

                app->nfc_listener = nfc_listener_alloc(app->nfc, protocol, data);

                if (app->nfc_listener) {
                    view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventStartNfc);
                }
            }
            return true;
        }

        case NfcQrCustomEventStartNfc: {
            nfc_listener_start(app->nfc_listener, NULL, NULL);

            // Ensure QR is loaded if not already
            if (!app->qrcode && !furi_string_empty(app->qr_file_path)) {
                load_qr_code(app);
            }

            // Show QR
            if (app->emulate_view) {
                with_view_model(
                    app->emulate_view,
                    NfcQrAppModel * model,
                    {
                        model->qrcode = app->qrcode;
                    },
                    true);
            }
            return true;
        }
        case NfcQrCustomEventMenuCredits: {
            widget_reset(app->widget);
            widget_add_text_scroll_element(
                app->widget,
                0,
                0,
                128,
                64,
                "Made by: DonJulve\n\n"
                "GitHub:\n"
                "github.com/DonJulve\n\n"
                "LinkedIn:\n"
                "linkedin.com/in/\njavier-julve-yubero/");

            app->current_view = NfcQrAppViewWidget;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewWidget);
            return true;
        }
        case NfcQrCustomEventEmulateBack: {
            if (app->nfc_listener) {
                nfc_listener_stop(app->nfc_listener);
                nfc_listener_free(app->nfc_listener);
                app->nfc_listener = NULL;
            }
            app->current_view = NfcQrAppViewSubmenu;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewSubmenu);
            return true;
        }
    }
    return false;
}

// Navigation Callback
static bool navigation_event_callback(void* context) {
    NfcQrApp* app = context;
    if (app->current_view == NfcQrAppViewWidget) {
        if (app->popup_timer) {
            furi_timer_stop(app->popup_timer);
        }
        app->current_view = NfcQrAppViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewSubmenu);
        return true;
    } else if (app->current_view == NfcQrAppViewTextInput) {
        app->current_view = NfcQrAppViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewSubmenu);
        return true;
    } else if (app->current_view == NfcQrAppViewSubmenu) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    return false;
}

// Main Entry
int32_t networking_nfc_qr_app_entry(void* p) {
    UNUSED(p);
    NfcQrApp* app = malloc(sizeof(NfcQrApp));
    memset(app, 0, sizeof(NfcQrApp));

    app->nfc_file_path = furi_string_alloc();
    app->qr_file_path = furi_string_alloc();
    app->popup_timer = furi_timer_alloc(result_timer_callback, FuriTimerTypeOnce, app);

    // GUI Setup
    app->view_dispatcher = view_dispatcher_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_event_callback);

    // Submenu Setup
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Networking NFC + QR");
    submenu_add_item(
        app->submenu, "Select QR Code", NfcQrCustomEventMenuSelectQr, submenu_callback, app->view_dispatcher);
    submenu_add_item(
        app->submenu, "Select NFC File", NfcQrCustomEventMenuSelectNfc, submenu_callback, app->view_dispatcher);
    submenu_add_item(
        app->submenu, "Start Emulation", NfcQrCustomEventMenuStart, submenu_callback, app->view_dispatcher);
    submenu_add_item(
        app->submenu, "Create QR + NFC", NfcQrCustomEventMenuCreate, submenu_callback, app->view_dispatcher);
    submenu_add_item(
        app->submenu, "Credits", NfcQrCustomEventMenuCredits, submenu_callback, app->view_dispatcher);
    view_dispatcher_add_view(app->view_dispatcher, NfcQrAppViewSubmenu, submenu_get_view(app->submenu));

    // Widget Setup (Credits, Alerts, Results)
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NfcQrAppViewWidget, widget_get_view(app->widget));

    // Text Input Setup
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcQrAppViewTextInput, text_input_get_view(app->text_input));

    // Emulation View Setup
    app->emulate_view = view_alloc();
    view_allocate_model(app->emulate_view, ViewModelTypeLockFree, sizeof(NfcQrAppModel));
    view_set_draw_callback(app->emulate_view, emulate_view_draw_callback);
    view_set_input_callback(app->emulate_view, emulate_view_input_callback);
    view_set_context(app->emulate_view, app);
    view_dispatcher_add_view(app->view_dispatcher, NfcQrAppViewEmulate, app->emulate_view);

    // NFC Setup
    app->nfc = nfc_alloc();
    if (!app->nfc) {
        FURI_LOG_E(TAG, "Failed to alloc NFC");
        view_dispatcher_free(app->view_dispatcher);
        furi_record_close(RECORD_GUI);
        furi_string_free(app->nfc_file_path);
        furi_string_free(app->qr_file_path);
        free(app);
        return 0;
    }

    app->nfc_device = nfc_device_alloc();
    if (!app->nfc_device) {
        FURI_LOG_E(TAG, "Failed to alloc NFC Device");
        nfc_free(app->nfc);
        view_dispatcher_free(app->view_dispatcher);
        furi_record_close(RECORD_GUI);
        furi_string_free(app->nfc_file_path);
        furi_string_free(app->qr_file_path);
        free(app);
        return 0;
    }

    // Ensure directories exist
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, "/ext/apps_data");
    storage_simply_mkdir(storage, "/ext/apps_data/qrcodes");
    storage_simply_mkdir(storage, "/ext/nfc");
    furi_record_close(RECORD_STORAGE);

    // Set default paths
    furi_string_set(app->qr_file_path, "/ext/apps_data/qrcodes");
    furi_string_set(app->nfc_file_path, "/ext/nfc");

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    // Start with Submenu
    app->current_view = NfcQrAppViewSubmenu;
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewSubmenu);

    view_dispatcher_run(app->view_dispatcher);

    // Cleanup
    if (app->nfc_listener) {
        nfc_listener_stop(app->nfc_listener);
        nfc_listener_free(app->nfc_listener);
    }

    if (app->qrcode) {
        free(app->qrcode->modules);
        free(app->qrcode);
    }
    if (app->nfc_device) nfc_device_free(app->nfc_device);
    if (app->nfc) nfc_free(app->nfc);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);

    view_dispatcher_remove_view(app->view_dispatcher, NfcQrAppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, NfcQrAppViewEmulate);
    view_dispatcher_remove_view(app->view_dispatcher, NfcQrAppViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, NfcQrAppViewTextInput);
    submenu_free(app->submenu);
    view_free(app->emulate_view);
    widget_free(app->widget);
    text_input_free(app->text_input);
    view_dispatcher_free(app->view_dispatcher);

    if (app->popup_timer) {
        furi_timer_stop(app->popup_timer);
        furi_timer_free(app->popup_timer);
    }

    furi_string_free(app->nfc_file_path);
    furi_string_free(app->qr_file_path);
    free(app);

    return 0;
}
