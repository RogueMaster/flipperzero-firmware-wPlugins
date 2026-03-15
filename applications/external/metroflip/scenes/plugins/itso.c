/* itso.c - Parser for ITSO cards (United Kingdom). */
#include "../../metroflip_i.h"
#include <flipper_application.h>

#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include <lib/nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include <lib/toolbox/strint.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#include <applications/services/locale/locale.h>
#include <datetime.h>

#define TAG "Metroflip:Scene:ITSO"

static const MfDesfireApplicationId itso_app_id = {.data = {0x16, 0x02, 0xa0}};
static const MfDesfireFileId itso_file_id = 0x0f;

int64_t swap_int64(int64_t val) {
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
    return (val << 32) | ((val >> 32) & 0xFFFFFFFFULL);
}

uint64_t swap_uint64(uint64_t val) {
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL) | ((val >> 8) & 0x00FF00FF00FF00FFULL);
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL) | ((val >> 16) & 0x0000FFFF0000FFFFULL);
    return (val << 32) | (val >> 32);
}

bool itso_parse(const MfDesfireData* data, FuriString* parsed_data) {
    furi_assert(parsed_data);

    bool parsed = false;

    do {
        const MfDesfireApplication* app = mf_desfire_get_application(data, &itso_app_id);
        if(app == NULL) break;

        typedef struct {
            uint64_t part1;
            uint64_t part2;
            uint64_t part3;
            uint64_t part4;
        } ItsoFile;

        const MfDesfireFileSettings* file_settings =
            mf_desfire_get_file_settings(app, &itso_file_id);

        if(file_settings == NULL || file_settings->type != MfDesfireFileTypeStandard ||
           file_settings->data.size < sizeof(ItsoFile))
            break;

        const MfDesfireFileData* file_data = mf_desfire_get_file_data(app, &itso_file_id);
        if(file_data == NULL) break;

        const ItsoFile* itso_file = simple_array_cget_data(file_data->data);

        uint64_t x1 = swap_uint64(itso_file->part1);
        uint64_t x2 = swap_uint64(itso_file->part2);

        char cardBuff[32];
        char dateBuff[18];

        snprintf(cardBuff, sizeof(cardBuff), "%llx%llx", x1, x2);
        snprintf(dateBuff, sizeof(dateBuff), "%llx", x2);

        char* cardp = cardBuff + 4;
        cardp[18] = '\0';

        // All itso card numbers are prefixed with "633597"
        if(strncmp(cardp, "633597", 6) != 0) break;

        char* datep = dateBuff + 12;
        dateBuff[17] = '\0';

        // DateStamp is defined in BS EN 1545 - Days passed since 01/01/1997
        uint32_t dateStamp;
        if(strint_to_uint32(datep, NULL, &dateStamp, 16) != StrintParseNoError) {
            return false;
        }
        uint32_t unixTimestamp = dateStamp * 24 * 60 * 60 + 852076800U;

        furi_string_set(parsed_data, "\e#ITSO Card\n");

        // Digit count in each space-separated group
        static const uint8_t digit_count[] = {6, 4, 4, 4};

        for(uint32_t i = 0, k = 0; i < COUNT_OF(digit_count); k += digit_count[i++]) {
            for(uint32_t j = 0; j < digit_count[i]; ++j) {
                furi_string_push_back(parsed_data, cardp[j + k]);
            }
            furi_string_push_back(parsed_data, ' ');
        }

        DateTime timestamp = {0};
        datetime_timestamp_to_datetime(unixTimestamp, &timestamp);

        FuriString* timestamp_str = furi_string_alloc();
        locale_format_date(timestamp_str, &timestamp, locale_get_date_format(), "-");

        furi_string_cat(parsed_data, "\nExpiry: ");
        furi_string_cat(parsed_data, timestamp_str);

        furi_string_free(timestamp_str);

        parsed = true;
    } while(false);

    return parsed;
}

static NfcCommand itso_poller_callback(NfcGenericEvent event, void* context) {
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
        if(!itso_parse(data, parsed_data)) {
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

static void itso_on_enter(Metroflip* app) {
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
            if(!itso_parse(data, parsed_data)) {
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
        nfc_poller_start(app->poller, itso_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool itso_on_event(Metroflip* app, SceneManagerEvent event) {
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

static void itso_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    metroflip_app_blink_stop(app);
    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin itso_plugin = {
    .card_name = "ITSO",
    .plugin_on_enter = itso_on_enter,
    .plugin_on_event = itso_on_event,
    .plugin_on_exit = itso_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor itso_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &itso_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* itso_plugin_ep(void) {
    return &itso_plugin_descriptor;
}
