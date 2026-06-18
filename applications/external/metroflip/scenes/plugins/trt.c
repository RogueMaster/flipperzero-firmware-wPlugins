// Flipper Zero parser for for Tianjin Railway Transit (TRT)
// https://en.wikipedia.org/wiki/Tianjin_Metro
// Reverse engineering and parser development by @Torron (Github: @zinongli) and added to Metroflip by @Lupin (Github: @luu176)

#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>

#define TAG                       "Metroflip:Scene:TrtParser"
#define LATEST_SALE_MARKER        0x02
#define FULL_SALE_TIME_STAMP_PAGE 0x09
#define BALANCE_PAGE              0x08
#define SALE_RECORD_TIME_STAMP_A  0x0C
#define SALE_RECORD_TIME_STAMP_B  0x0E
#define SALE_YEAR_OFFSET          2000

static bool trt_display_card_view(const MfUltralightData* data, Metroflip* app, bool from_file) {
    furi_assert(data);

    uint8_t latest_sale_page = 0;

    // Look for sale record signature
    if(data->page[SALE_RECORD_TIME_STAMP_A].data[0] == LATEST_SALE_MARKER) {
        latest_sale_page = SALE_RECORD_TIME_STAMP_A;
    } else if(data->page[SALE_RECORD_TIME_STAMP_B].data[0] == LATEST_SALE_MARKER) {
        latest_sale_page = SALE_RECORD_TIME_STAMP_B;
    } else {
        return false;
    }

    // Check if the sale record was backed up
    const uint8_t* partial_record_pointer = &data->page[latest_sale_page - 1].data[0];
    const uint8_t* full_record_pointer = &data->page[FULL_SALE_TIME_STAMP_PAGE].data[0];
    uint32_t latest_sale_record = bit_lib_get_bits_32(partial_record_pointer, 3, 20);
    uint32_t latest_sale_full_record = bit_lib_get_bits_32(full_record_pointer, 0, 27);
    if(latest_sale_record != (latest_sale_full_record & 0xFFFFF))
        return false; // check if the copy matches
    if((latest_sale_record == 0) || (latest_sale_full_record == 0))
        return false; // prevent false positive

    // Parse date
    // yyy yyyymmmm dddddhhh hhnnnnnn
    uint16_t sale_year = ((latest_sale_full_record & 0x7F00000) >> 20) + SALE_YEAR_OFFSET;
    uint8_t sale_month = (latest_sale_full_record & 0xF0000) >> 16;
    uint8_t sale_day = (latest_sale_full_record & 0xF800) >> 11;
    uint8_t sale_hour = (latest_sale_full_record & 0x7C0) >> 6;
    uint8_t sale_minute = latest_sale_full_record & 0x3F;

    // Parse balance
    uint16_t balance = bit_lib_get_bits_16(&data->page[BALANCE_PAGE].data[2], 0, 16);
    uint16_t balance_yuan = balance / 100;
    uint8_t balance_cent = balance % 100;

    // Build card view
    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "TRT Tianjin");

    uint8_t p = metroflip_card_view_add_page(view, "Ticket Info");

    metroflip_card_view_add_field(view, p, "Type", "Single-Use Ticket", false);

    snprintf(val, sizeof(val), "%u.%02u RMB", balance_yuan, balance_cent);
    metroflip_card_view_add_field(view, p, "Balance", val, true);

    snprintf(
        val,
        sizeof(val),
        "%04u-%02d-%02d %02d:%02d",
        sale_year,
        sale_month,
        sale_day,
        sale_hour,
        sale_minute);
    metroflip_card_view_add_field(view, p, "Sale Date", val, false);

    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

static NfcCommand trt_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfUltralight);

    Metroflip* app = context;
    const MfUltralightPollerEvent* mf_ultralight_event = event.event_data;

    if(mf_ultralight_event->type == MfUltralightPollerEventTypeReadSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfUltralight, nfc_poller_get_data(app->poller));

        const MfUltralightData* data =
            nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
        uint32_t event = (data->pages_read == data->pages_total) ?
                             MetroflipCustomEventPollerSuccess :
                             MetroflipCustomEventPollerFail;
        view_dispatcher_send_custom_event(app->view_dispatcher, event);
        return NfcCommandStop;
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthRequest) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
    } else if(mf_ultralight_event->type == MfUltralightPollerEventTypeAuthSuccess) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
    }

    return NfcCommandContinue;
}

static void trt_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        FURI_LOG_I(TAG, "TRT loaded");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfUltralightData* ultralight_data = mf_ultralight_alloc();
            mf_ultralight_load(ultralight_data, ff, 2);

            if(!trt_display_card_view(ultralight_data, app, true)) {
                FURI_LOG_I(TAG, "Unknown card type");
                Widget* widget = app->widget;
                FuriString* s = furi_string_alloc_set("\e#Unknown card\n");
                widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
                widget_add_button_element(
                    widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                furi_string_free(s);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            }

            mf_ultralight_free(ultralight_data);
        } else {
            FURI_LOG_E(TAG, "Failed to open saved file: %s", app->file_path);
            Widget* widget = app->widget;
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, "\e#Error\nFailed to open\nsaved file.");
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
    } else {
        FURI_LOG_I(TAG, "TRT not loaded");
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(
            popup, "Scanning...\nApply card\nto the back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
        nfc_poller_start(app->poller, trt_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool trt_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card found!\nDon't move...", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerSuccess) {
            const MfUltralightData* ultralight_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);

            if(!trt_display_card_view(ultralight_data, app, false)) {
                FURI_LOG_I(TAG, "Unknown card type");
                Widget* widget = app->widget;
                FuriString* s = furi_string_alloc_set("\e#Unknown card\n");
                widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
                widget_add_button_element(
                    widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                furi_string_free(s);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            }

            metroflip_app_blink_stop(app);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card lost!\nTry again", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Wrong card", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            FURI_LOG_I(TAG, "Unknown card type");
            Widget* widget = app->widget;
            FuriString* s = furi_string_alloc_set("\e#Unknown card\n");
            widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            furi_string_free(s);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            metroflip_app_blink_stop(app);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void trt_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin trt_plugin = {
    .card_name = "TRT",
    .plugin_on_enter = trt_on_enter,
    .plugin_on_event = trt_on_event,
    .plugin_on_exit = trt_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor trt_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &trt_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* trt_plugin_ep(void) {
    return &trt_plugin_descriptor;
}
