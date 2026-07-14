
#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:Metromoney"

const MfClassicKeyPair metromoney_1k_keys[16] = {
    {.a = 0x2803BCB0C7E1, .b = 0x4FA9EB49F75E},
    {.a = 0x9C616585E26D, .b = 0xD1C71E590D16},
    {.a = 0x9C616585E26D, .b = 0xA160FCD5EC4C},
    {.a = 0x9C616585E26D, .b = 0xA160FCD5EC4C},
    {.a = 0x9C616585E26D, .b = 0xA160FCD5EC4C},
    {.a = 0x9C616585E26D, .b = 0xA160FCD5EC4C},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.a = 0x112233445566, .b = 0x361A62F35BC9},
    {.a = 0x112233445566, .b = 0x361A62F35BC9},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
    {.a = 0xFFFFFFFFFFFF, .b = 0xFFFFFFFFFFFF},
};

/* Parse Metromoney data and populate card view. Returns true on success. */
static bool
    metromoney_display_card_view(const MfClassicData* data, Metroflip* app, bool from_file) {
    // Verify key
    const uint8_t ticket_sector_number = 1;
    const uint8_t ticket_block_number = 1;

    const MfClassicSectorTrailer* sec_tr =
        mf_classic_get_sector_trailer_by_sector(data, ticket_sector_number);

    const uint64_t key = bit_lib_bytes_to_num_be(sec_tr->key_a.data, COUNT_OF(sec_tr->key_a.data));
    if(key != metromoney_1k_keys[ticket_sector_number].a) return false;

    FURI_LOG_D(TAG, "passed key check");

    // Parse data
    const uint8_t start_block_num = mf_classic_get_first_block_num_of_sector(ticket_sector_number);

    const uint8_t* block_start_ptr = &data->block[start_block_num + ticket_block_number].data[0];

    uint32_t balance = bit_lib_bytes_to_num_le(block_start_ptr, 4) - 100;

    uint32_t balance_lari = balance / 100;
    uint8_t balance_tetri = balance % 100;

    size_t uid_len = 0;
    const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
    uint32_t card_number = bit_lib_bytes_to_num_le(uid, 4);

    /* Allocate card view */
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "Metromoney");

    /* Page: Card Info */
    uint8_t p = metroflip_card_view_add_page(view, "Card Info");
    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    snprintf(val, sizeof(val), "%lu", card_number);
    metroflip_card_view_add_field(view, p, "Card Number", val, false);

    snprintf(val, sizeof(val), "%lu.%02u GEL", balance_lari, balance_tetri);
    metroflip_card_view_add_field(view, p, "Balance", val, true);

    /* Button configuration */
    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

static NfcCommand metromoney_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    const MfClassicPollerEvent* mfc_event = event.event_data;
    Metroflip* app = context;

    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardDetected);
        command = NfcCommandContinue;
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventCardLost);
        app->sec_num = 0;
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;

    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        MfClassicKey key = {0};
        bit_lib_num_to_bytes_be(metromoney_1k_keys[app->sec_num].a, COUNT_OF(key.data), key.data);

        MfClassicKeyType key_type = MfClassicKeyTypeA;
        mfc_event->data->read_sector_request_data.sector_num = app->sec_num;
        mfc_event->data->read_sector_request_data.key = key;
        mfc_event->data->read_sector_request_data.key_type = key_type;
        mfc_event->data->read_sector_request_data.key_provided = true;
        if(app->sec_num == 16) {
            mfc_event->data->read_sector_request_data.key_provided = false;
            app->sec_num = 0;
        }
        app->sec_num++;
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        /* Hand off to the main thread - building/registering/switching the
           card view must not happen on the NFC worker thread. */
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

static void metromoney_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);
    FURI_LOG_I(TAG, "open metromoney");
    app->sec_num = 0;

    if(app->data_loaded) {
        FURI_LOG_I(TAG, "tbilisi loaded");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);

            if(!metromoney_display_card_view(mfc_data, app, true)) {
                FURI_LOG_I(TAG, "Unknown card type");
                Widget* widget = app->widget;
                FuriString* s = furi_string_alloc_set("\e#Unknown card\n");
                widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(s));
                widget_add_button_element(
                    widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
                furi_string_free(s);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
            }

            mf_classic_free(mfc_data);
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
        FURI_LOG_I(TAG, "tbilisi not loaded");
        Popup* popup = app->popup;
        popup_set_header(
            popup, "Scanning...\nApply card\nto the back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, metromoney_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool metromoney_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Read finished on the worker thread; build the card view here on
               the main/GUI thread. */
            metroflip_app_blink_stop(app);
            dolphin_deed(DolphinDeedNfcReadSuccess);
            const MfClassicData* mfc_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
            if(!metromoney_display_card_view(mfc_data, app, false)) {
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

static void metromoney_on_exit(Metroflip* app) {
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
static const MetroflipPlugin metromoney_plugin = {
    .card_name = "Metromoney",
    .plugin_on_enter = metromoney_on_enter,
    .plugin_on_event = metromoney_on_event,
    .plugin_on_exit = metromoney_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor metromoney_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &metromoney_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* metromoney_plugin_ep(void) {
    return &metromoney_plugin_descriptor;
}
