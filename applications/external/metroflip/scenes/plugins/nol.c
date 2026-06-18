#include <flipper_application.h>

#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include <stdio.h>
#include <lib/bit_lib/bit_lib.h>

#include "../../metroflip_i.h"
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG           "Metroflip:Scene:nol"
#define NOL_FILE_SIZE 64

static const MfDesfireApplicationId nol_app_id = {.data = {0xff, 0xff, 0xff}};
static const MfDesfireFileId nol_file_id = 0x08;

/* Parse nol card data and populate card view. Returns true on success. */
static bool nol_display_card_view(const MfDesfireData* data, Metroflip* app, bool from_file) {
    const MfDesfireApplication* mf_app = mf_desfire_get_application(data, &nol_app_id);
    if(mf_app == NULL) return false;

    const MfDesfireFileSettings* file_settings =
        mf_desfire_get_file_settings(mf_app, &nol_file_id);

    if(file_settings == NULL || file_settings->type != MfDesfireFileTypeStandard ||
       file_settings->data.size < NOL_FILE_SIZE)
        return false;

    const MfDesfireFileData* file_data = mf_desfire_get_file_data(mf_app, &nol_file_id);
    if(file_data == NULL) return false;

    uint8_t* nol_file = simple_array_get_data(file_data->data);
    uint32_t nol_serial_number = bit_lib_get_bits_32(nol_file, 61, 32);

    uint32_t first_group = (nol_serial_number / 10000000U) % 1000U;
    uint32_t middle_group = (nol_serial_number / 10000U) % 1000U;
    uint32_t last_group = nol_serial_number % 10000U;

    /* Allocate card view */
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "nol");

    /* Page: Card Info */
    uint8_t p = metroflip_card_view_add_page(view, "Card Info");
    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    snprintf(val, sizeof(val), "%03lu %03lu %04lu", first_group, middle_group, last_group);
    metroflip_card_view_add_field(view, p, "Serial No.", val, true);

    /* Button configuration */
    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

static NfcCommand nol_poller_callback(NfcGenericEvent event, void* context) {
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

static void nol_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfDesfireData* data = mf_desfire_alloc();
            mf_desfire_load(data, ff, 2);

            if(!nol_display_card_view(data, app, true)) {
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
        Popup* popup = app->popup;
        popup_set_header(
            popup, "Scanning...\nApply card\nto the back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfDesfire);
        nfc_poller_start(app->poller, nol_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool nol_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Read finished on the worker thread; build the card view here on
               the main/GUI thread. */
            metroflip_app_blink_stop(app);
            const MfDesfireData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfDesfire);
            if(!nol_display_card_view(data, app, false)) {
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

static void nol_on_exit(Metroflip* app) {
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
static const MetroflipPlugin nol_plugin = {
    .card_name = "nol",
    .plugin_on_enter = nol_on_enter,
    .plugin_on_event = nol_on_event,
    .plugin_on_exit = nol_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor nol_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &nol_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* nol_plugin_ep(void) {
    return &nol_plugin_descriptor;
}
