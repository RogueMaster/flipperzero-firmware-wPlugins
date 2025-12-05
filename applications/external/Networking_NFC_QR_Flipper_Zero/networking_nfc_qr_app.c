#include "networking_nfc_qr_app.h"
#include <furi.h>
#include <furi_hal_nfc.h>
#include <gui/gui.h>
#include <notification/notification_messages.h>

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
        
        // Try Version 3 first (max 44 bytes at Medium) - Bigger pixels
        uint8_t version = 3;
        uint8_t* modules = malloc(qrcode_getBufferSize(version));
        memset(modules, 0, qrcode_getBufferSize(version));
        app->qrcode->modules = modules;
        
        if (qrcode_initBytes(app->qrcode, modules, MODE_BYTE, version, ECC_MEDIUM, (uint8_t*)furi_string_get_cstr(temp_str), furi_string_size(temp_str)) != 0) {
            // Failed, try Version 4 (max 64 bytes at Medium)
            free(modules);
            version = 4;
            modules = malloc(qrcode_getBufferSize(version));
            memset(modules, 0, qrcode_getBufferSize(version));
            app->qrcode->modules = modules;
            
            if (qrcode_initBytes(app->qrcode, modules, MODE_BYTE, version, ECC_MEDIUM, (uint8_t*)furi_string_get_cstr(temp_str), furi_string_size(temp_str)) != 0) {
                // Failed again
                free(modules);
                free(app->qrcode);
                app->qrcode = NULL;
                break;
            }
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




// Helper to check if protocol has a listener
static bool is_protocol_supported(NfcProtocol protocol) {
    switch(protocol) {
        case NfcProtocolIso14443_3a:
        case NfcProtocolIso14443_4a:
        case NfcProtocolIso15693_3:
        case NfcProtocolFelica:
        case NfcProtocolMfUltralight:
        case NfcProtocolMfClassic:
        case NfcProtocolSlix:
            return true;
        default:
            return false;
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
        case NfcQrCustomEventMenuStart: {
            // Check if files are selected
            if(furi_string_empty(app->qr_file_path)) {
                // Show error or something
                return true;
            }
            if(furi_string_empty(app->nfc_file_path)) {
                // Show error
                return true;
            }

            // Validate extension
            if(!furi_string_end_with(app->nfc_file_path, ".nfc")) {
                FURI_LOG_E(TAG, "Invalid NFC file extension");
                return true;
            }

            // Check HAL Ready
            if(furi_hal_nfc_is_hal_ready() != FuriHalNfcErrorNone) {
                 DialogMessage* message = dialog_message_alloc();
                 dialog_message_set_text(message, "NFC Hardware Busy\nTry again later", 64, 32, AlignCenter, AlignCenter);
                 dialog_message_set_buttons(message, "OK", NULL, NULL);
                 dialog_message_show(app->dialogs, message);
                 dialog_message_free(message);
                 return true;
            }

            // Alloc NFC Device
            if (!app->nfc_device) {
                app->nfc_device = nfc_device_alloc();
                if (!app->nfc_device) {
                    // Handle error
                    return true;
                }
            }
            
            // Alloc NFC
            if (!app->nfc) {
                 app->nfc = nfc_alloc();
            }
            
            // Switch to emulate view first
            app->current_view = NfcQrAppViewEmulate;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewEmulate);
            
            // Start the chain
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventLoadFile);
            return true;
        }
        
        case NfcQrCustomEventLoadFile: {
            if (nfc_device_load(app->nfc_device, furi_string_get_cstr(app->nfc_file_path))) {
                view_dispatcher_send_custom_event(app->view_dispatcher, NfcQrCustomEventAllocNfc);
            }
            return true;
        }
        


        case NfcQrCustomEventAllocNfc: {
            NfcProtocol protocol = nfc_device_get_protocol(app->nfc_device);
            
            if (protocol == NfcProtocolInvalid) {
                 return true;
            }
            
            const NfcDeviceData* data = NULL;

            // Try to use the native protocol if supported
            if (is_protocol_supported(protocol)) {
                data = nfc_device_get_data(app->nfc_device, protocol);
            } 
            // Fallback: Check for parent protocols if native is not supported (e.g. EMV -> ISO14443_4A)
            else {
                if (nfc_protocol_has_parent(protocol, NfcProtocolIso14443_4a)) {
                    protocol = NfcProtocolIso14443_4a;
                    data = nfc_device_get_data(app->nfc_device, protocol);
                } else if (nfc_protocol_has_parent(protocol, NfcProtocolIso14443_3a)) {
                    protocol = NfcProtocolIso14443_3a;
                    data = nfc_device_get_data(app->nfc_device, protocol);
                }
            }
            
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
                        model->qrcode = app->qrcode; // Ensure QR is set
                    },
                    true);
            }
            return true;
        }
        case NfcQrCustomEventMenuCredits: {
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, 
                "Made by: DonJulve\n\n"
                "GitHub:\n"
                "github.com/DonJulve\n\n"
                "LinkedIn:\n"
                "linkedin.com/in/\njavier-julve-yubero-\n188203384/");
            
            app->current_view = NfcQrAppViewCredits;
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewCredits);
            return true;
        }
        case NfcQrCustomEventEmulateBack: {
            if (app->nfc_listener) {
                nfc_listener_stop(app->nfc_listener);
                nfc_listener_free(app->nfc_listener);
                app->nfc_listener = NULL;
            }
            if (app->nfc) {
                nfc_free(app->nfc);
                app->nfc = NULL;
            }
            if (app->nfc_device) {
                nfc_device_free(app->nfc_device);
                app->nfc_device = NULL;
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
    if (app->current_view == NfcQrAppViewCredits) {
        app->current_view = NfcQrAppViewSubmenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcQrAppViewSubmenu);
    } else {
        view_dispatcher_stop(app->view_dispatcher);
    }
    return true;
}

// Main Entry
int32_t networking_nfc_qr_app_entry(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "App starting...");
    
    NfcQrApp* app = malloc(sizeof(NfcQrApp));
    memset(app, 0, sizeof(NfcQrApp));
    
    FURI_LOG_I(TAG, "App struct allocated");

    app->nfc_file_path = furi_string_alloc();
    app->qr_file_path = furi_string_alloc();
    
    FURI_LOG_I(TAG, "Strings allocated");

    // GUI Setup
    app->view_dispatcher = view_dispatcher_alloc();
    app->gui = furi_record_open(RECORD_GUI);
    
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, navigation_event_callback);
    
    // Submenu Setup
    app->submenu = submenu_alloc();
    submenu_set_header(app->submenu, "Networking NFC + QR");
    submenu_add_item(app->submenu, "Select QR Code", NfcQrCustomEventMenuSelectQr, submenu_callback, app->view_dispatcher);
    submenu_add_item(app->submenu, "Select NFC File", NfcQrCustomEventMenuSelectNfc, submenu_callback, app->view_dispatcher);
    submenu_add_item(app->submenu, "Start Emulation", NfcQrCustomEventMenuStart, submenu_callback, app->view_dispatcher);
    submenu_add_item(app->submenu, "Credits", NfcQrCustomEventMenuCredits, submenu_callback, app->view_dispatcher);
    
    view_dispatcher_add_view(app->view_dispatcher, NfcQrAppViewSubmenu, submenu_get_view(app->submenu));
    
    // Widget Setup (Credits)
    app->widget = widget_alloc();
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "made by: DonJulve\nGitHub: DonJulve\nLinkedIn: julve");
    view_dispatcher_add_view(app->view_dispatcher, NfcQrAppViewCredits, widget_get_view(app->widget));
    
    // Emulation View Setup
    app->emulate_view = view_alloc();
    view_allocate_model(app->emulate_view, ViewModelTypeLockFree, sizeof(NfcQrAppModel));
    view_set_draw_callback(app->emulate_view, emulate_view_draw_callback);
    view_set_input_callback(app->emulate_view, emulate_view_input_callback);
    view_set_context(app->emulate_view, app);
    view_dispatcher_add_view(app->view_dispatcher, NfcQrAppViewEmulate, app->emulate_view);
    
    // Attach to GUI
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Set default paths
    furi_string_set(app->qr_file_path, "/ext/apps_data/qrcodes");
    furi_string_set(app->nfc_file_path, "/ext/nfc");
    
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    
    // Start with Submenu
    app->current_view = NfcQrAppViewSubmenu;
    
    // Enable queue before switching view
    view_dispatcher_enable_queue(app->view_dispatcher);
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
    view_dispatcher_remove_view(app->view_dispatcher, NfcQrAppViewCredits);
    submenu_free(app->submenu);
    view_free(app->emulate_view);
    widget_free(app->widget);
    view_dispatcher_free(app->view_dispatcher);
    
    furi_string_free(app->nfc_file_path);
    furi_string_free(app->qr_file_path);
    free(app);
    
    return 0;
}
