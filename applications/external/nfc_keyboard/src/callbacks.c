#include "callbacks.h"

void nfc_hid_render_callback(Canvas* canvas, void* ctx) {
    NfcHidApp* app = ctx;

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, "NFC HID Scanner");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 10, "v");
    canvas_draw_str(canvas, 96, 10, VERSION);
    canvas_draw_str(canvas, 0, 20, "Scan a NFC Card");

    canvas_draw_str(canvas, 0, 30, "UID: ");

    canvas_draw_str(canvas, 40, 30, furi_string_get_cstr(app->uid_str));

    canvas_draw_str(canvas, 0, 63, "Press [back] to exit");
}

void nfc_hid_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    NfcHidApp* app = ctx;

    if(input_event->type == InputTypePress && input_event->key == InputKeyBack) {
        app->running = false;
    }
}

NfcCommand nfc_hid_poller_callback(NfcGenericEvent event, void* ctx) {
    furi_assert(ctx);
    NfcHidApp* app = ctx;

    nfc_device_set_data(app->device, event.protocol, nfc_poller_get_data(app->poller));

    app->detected = true;

    return NfcCommandStop;
}

void nfc_hid_scanner_callback(NfcScannerEvent event, void* ctx) {
    furi_assert(ctx);
    NfcHidApp* app = ctx;

    if(event.type == NfcScannerEventTypeDetected) {
        size_t hasKnownProtocol = 1000;
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            if(event.data.protocols[i] == NfcProtocolMfClassic) {
                hasKnownProtocol = i;
                break;
            }
            if(event.data.protocols[i] == NfcProtocolMfUltralight) {
                hasKnownProtocol = i;
                break;
            }
        }

        if(hasKnownProtocol != 1000) {
            app->poller = nfc_poller_alloc(app->nfc, event.data.protocols[hasKnownProtocol]);
            nfc_poller_start(app->poller, nfc_hid_poller_callback, app);
        } else {
            nfc_device_clear(app->device);
            furi_string_set(app->uid_str, "No MfClassic or MfcUltralight");
        }
    }
}
