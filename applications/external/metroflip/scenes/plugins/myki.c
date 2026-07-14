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

/* Parse DESFire data and populate card view */
static bool myki_display_card_view(const MfDesfireData* data, Metroflip* app, bool from_file) {
    const MfDesfireApplication* mf_app = mf_desfire_get_application(data, &myki_app_id);
    if(mf_app == NULL) return false;

    typedef struct {
        uint32_t top;
        uint32_t bottom;
    } mykiFile;

    const MfDesfireFileSettings* file_settings =
        mf_desfire_get_file_settings(mf_app, &myki_file_id);

    if(file_settings == NULL || file_settings->type != MfDesfireFileTypeStandard ||
       file_settings->data.size < sizeof(mykiFile))
        return false;

    const MfDesfireFileData* file_data = mf_desfire_get_file_data(mf_app, &myki_file_id);
    if(file_data == NULL) return false;

    const mykiFile* myki_file = simple_array_cget_data(file_data->data);

    // All myki card numbers are prefixed with "308425"
    if(myki_file->top != 308425UL) return false;
    // Card numbers are always 15 digits in length
    if(myki_file->bottom < 10000000UL || myki_file->bottom >= 100000000UL) return false;

    uint64_t card_number = myki_file->top * 1000000000ULL + myki_file->bottom * 10UL;
    // Stored card number doesn't include check digit
    card_number += myki_calculate_luhn(card_number);

    // Format the card number string
    char card_string[20];
    snprintf(card_string, sizeof(card_string), "%llu", card_number);

    // Stylise card number according to the physical card: 1 5 4 4 1
    char card_display[METROFLIP_CARD_VIEW_VALUE_LEN];
    snprintf(
        card_display,
        sizeof(card_display),
        "%c %c%c%c%c%c %c%c%c%c %c%c%c%c %c",
        card_string[0],
        card_string[1],
        card_string[2],
        card_string[3],
        card_string[4],
        card_string[5],
        card_string[6],
        card_string[7],
        card_string[8],
        card_string[9],
        card_string[10],
        card_string[11],
        card_string[12],
        card_string[13],
        card_string[14]);

    /* Allocate card view */
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "myki");

    /* Page: Card Info */
    uint8_t p = metroflip_card_view_add_page(view, "Card Info");

    metroflip_card_view_add_field(view, p, "Card No.", card_display, false);

    /* Button configuration */
    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

static NfcCommand myki_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfDesfire);

    Metroflip* app = context;
    NfcCommand command = NfcCommandContinue;

    const MfDesfirePollerEvent* mf_desfire_event = event.event_data;
    if(mf_desfire_event->type == MfDesfirePollerEventTypeReadSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfDesfire, nfc_poller_get_data(app->poller));
        /* Hand off to the main thread - building/registering/switching the
           card view must not happen on the NFC worker thread. */
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        command = NfcCommandStop;
    } else if(mf_desfire_event->type == MfDesfirePollerEventTypeReadFailed) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
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

            if(!myki_display_card_view(data, app, true)) {
                FURI_LOG_I(TAG, "Unknown card type");
                Widget* widget = app->widget;
                FuriString* s = furi_string_alloc_set("\e#Unknown card\n");
                widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
                widget_add_button_element(
                    widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                furi_string_free(s);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            }

            mf_desfire_free(data);
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
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(
            popup, "Scanning...\nApply card\nto the back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfDesfire);
        nfc_poller_start(app->poller, myki_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool myki_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Read finished on the worker thread; build the card view here on
               the main/GUI thread. */
            metroflip_app_blink_stop(app);
            const MfDesfireData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfDesfire);
            if(!myki_display_card_view(data, app, false)) {
                FURI_LOG_I(TAG, "Unknown card type");
                Widget* widget = app->widget;
                widget_add_text_scroll_element(widget, 0, 0, 128, 64, "\e#Unknown card\n");
                widget_add_button_element(
                    widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            }
            consumed = true;
        } else if(event.event == MetroflipCustomEventCardDetected) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Card found!\nDon't move...", 68, 30, AlignLeft, AlignTop);
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
            Popup* popup = app->popup;
            popup_set_header(popup, "Read failed", 68, 30, AlignLeft, AlignTop);
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
    popup_reset(app->popup);
    metroflip_app_blink_stop(app);
    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
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
