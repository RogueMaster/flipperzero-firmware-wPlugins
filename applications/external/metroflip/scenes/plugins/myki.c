#include <flipper_application.h>

#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include <stdio.h>

#include "../../metroflip_i.h"
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:myki"

static const MfDesfireApplicationId myki_app_id = {.data = {0x00, 0x11, 0xf2}};
static const MfDesfireFileId myki_file_id = 0x0f;

static uint8_t myki_calculate_luhn(uint64_t number) {
    // https://en.wikipedia.org/wiki/Luhn_algorithm
    // Drop existing check digit to form payload
    uint64_t payload = number / 10;
    int sum = 0;
    int position = 0;

    while(payload > 0) {
        int digit = payload % 10;
        if(position % 2 == 0) {
            digit *= 2;
        }
        if(digit > 9) {
            digit = (digit / 10) + (digit % 10);
        }
        sum += digit;
        payload /= 10;
        position++;
    }

    return (10 - (sum % 10)) % 10;
}

bool myki_parse(const MfDesfireData* data, FuriString* parsed_data) {
    furi_assert(parsed_data);

    bool parsed = false;

    do {
        const MfDesfireApplication* app = mf_desfire_get_application(data, &myki_app_id);
        if(app == NULL) break;

        typedef struct {
            uint32_t top;
            uint32_t bottom;
        } mykiFile;

        const MfDesfireFileSettings* file_settings =
            mf_desfire_get_file_settings(app, &myki_file_id);

        if(file_settings == NULL || file_settings->type != MfDesfireFileTypeStandard ||
           file_settings->data.size < sizeof(mykiFile))
            break;

        const MfDesfireFileData* file_data = mf_desfire_get_file_data(app, &myki_file_id);
        if(file_data == NULL) break;

        const mykiFile* myki_file = simple_array_cget_data(file_data->data);

        // All myki card numbers are prefixed with "308425"
        if(myki_file->top != 308425UL) break;
        // Card numbers are always 15 digits in length
        if(myki_file->bottom < 10000000UL || myki_file->bottom >= 100000000UL) break;

        uint64_t card_number = myki_file->top * 1000000000ULL + myki_file->bottom * 10UL;
        // Stored card number doesn't include check digit
        card_number += myki_calculate_luhn(card_number);

        furi_string_set(parsed_data, "\e#myki\nNo.: ");

        // Stylise card number according to the physical card
        char card_string[20];
        snprintf(card_string, sizeof(card_string), "%llu", card_number);

        // Digit count in each space-separated group
        static const uint8_t digit_count[] = {1, 5, 4, 4, 1};

        for(uint32_t i = 0, k = 0; i < COUNT_OF(digit_count); k += digit_count[i++]) {
            for(uint32_t j = 0; j < digit_count[i]; ++j) {
                furi_string_push_back(parsed_data, card_string[j + k]);
            }
            furi_string_push_back(parsed_data, ' ');
        }

        parsed = true;
    } while(false);

    return parsed;
}

static NfcCommand myki_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfDesfire);

    Metroflip* app = context;
    NfcCommand command = NfcCommandContinue;

    FuriString* parsed_data = furi_string_alloc();
    Widget* widget = app->widget;
    furi_string_reset(app->text_box_store);
    const MfDesfirePollerEvent* mf_desfire_event = event.event_data;
    if(mf_desfire_event->type == MfDesfirePollerEventTypeReadSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfDesfire, nfc_poller_get_data(app->poller));
        const MfDesfireData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfDesfire);
        if(!myki_parse(data, parsed_data)) {
            furi_string_reset(app->text_box_store);
            FURI_LOG_I(TAG, "Unknown card type");
            furi_string_printf(parsed_data, "\e#Unknown card\n");
        }
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        metroflip_app_blink_stop(app);
        command = NfcCommandStop;
    } else if(mf_desfire_event->type == MfDesfirePollerEventTypeReadFailed) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        command = NfcCommandContinue;
    }

    return command;
}

static void myki_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfDesfireData* data = mf_desfire_alloc();
            mf_desfire_load(data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!myki_parse(data, parsed_data)) {
                furi_string_reset(app->text_box_store);
                FURI_LOG_I(TAG, "Unknown card type");
                furi_string_printf(parsed_data, "\e#Unknown card\n");
            }
            widget_add_text_scroll_element(
                widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

            widget_add_button_element(
                widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
            widget_add_button_element(
                widget, GuiButtonTypeCenter, "Delete", metroflip_delete_widget_callback, app);
            mf_desfire_free(data);
            furi_string_free(parsed_data);
            view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        }
        flipper_format_free(ff);
    } else {
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        nfc_scanner_alloc(app->nfc);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfDesfire);
        nfc_poller_start(app->poller, myki_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool myki_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "DON'T\nMOVE", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardLost) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card \n lost", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventWrongCard) {
            Popup* popup = app->popup;
            popup_set_header(popup, "WRONG \n CARD", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Failed", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void myki_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    metroflip_app_blink_stop(app);
    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin myki_plugin = {
    .card_name = "Myki",
    .plugin_on_enter = myki_on_enter,
    .plugin_on_event = myki_on_event,
    .plugin_on_exit = myki_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor myki_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &myki_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* myki_plugin_ep(void) {
    return &myki_plugin_descriptor;
}
