#include "../../metroflip_i.h"

#include <bit_lib.h>
#include <flipper_application.h>
#include <furi.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include <string.h>
#include <dolphin/dolphin.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <storage/storage.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define MAX_TRIPS           10
#define TAG                 "Metroflip:Scene:Smartrider"
#define MAX_BLOCKS          64
#define MAX_DATE_ITERATIONS 366

uint8_t smartrider_sector_num = 0;

typedef struct {
    uint32_t timestamp;
    uint16_t cost;
    uint16_t transaction_number;
    uint16_t journey_number;
    char route[5];
    uint8_t tap_on : 1;
    uint8_t block;
} __attribute__((packed)) TripData;

typedef struct {
    uint32_t balance;
    uint16_t issued_days;
    uint16_t expiry_days;
    uint16_t purchase_cost;
    uint16_t auto_load_threshold;
    uint16_t auto_load_value;
    char card_serial_number[11];
    uint8_t token;
    TripData trips[MAX_TRIPS];
    uint8_t trip_count;
} __attribute__((packed)) SmartRiderData;

static const char* const CONCESSION_TYPES[] = {
    "Pre-issue",
    "Standard Fare",
    "Student",
    NULL,
    "Tertiary",
    NULL,
    "Seniors",
    "Health Care",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "PTA Staff",
    "Pensioner",
    "Free Travel"};

static inline const char* get_concession_type(uint8_t token) {
    return (token <= 0x10) ? CONCESSION_TYPES[token] : "Unknown";
}

static inline bool
    parse_trip_data(const MfClassicBlock* block_data, TripData* trip, uint8_t block_number) {
    trip->timestamp = bit_lib_bytes_to_num_le(block_data->data + 3, 4);
    trip->tap_on = (block_data->data[7] & 0x10) == 0x10;
    memcpy(trip->route, block_data->data + 8, 4);
    trip->route[4] = '\0';
    trip->cost = bit_lib_bytes_to_num_le(block_data->data + 13, 2);
    trip->transaction_number = bit_lib_bytes_to_num_le(block_data->data, 2);
    trip->journey_number = bit_lib_bytes_to_num_le(block_data->data + 2, 2);
    trip->block = block_number;
    return true;
}

static bool is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static void calculate_date(uint32_t timestamp, char* date_str, size_t date_str_size) {
    uint32_t seconds_since_2000 = timestamp;
    uint32_t days_since_2000 = seconds_since_2000 / 86400;
    uint16_t year = 2000;
    uint8_t month = 1;
    uint16_t day = 1;

    static const uint16_t days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    while(days_since_2000 >= (is_leap_year(year) ? 366 : 365)) {
        days_since_2000 -= (is_leap_year(year) ? 366 : 365);
        year++;
    }

    for(month = 0; month < 12; month++) {
        uint16_t dim = days_in_month[month];
        if(month == 1 && is_leap_year(year)) {
            dim++;
        }
        if(days_since_2000 < dim) {
            break;
        }
        days_since_2000 -= dim;
    }

    day = days_since_2000 + 1;
    month++; // Adjust month to 1-based

    if(date_str_size > 0) {
        size_t written = 0;
        written += snprintf(date_str + written, date_str_size - written, "%02u", day);
        if(written < date_str_size - 1) {
            written += snprintf(date_str + written, date_str_size - written, "/");
        }
        if(written < date_str_size - 1) {
            written += snprintf(date_str + written, date_str_size - written, "%02u", month);
        }
        if(written < date_str_size - 1) {
            written += snprintf(date_str + written, date_str_size - written, "/");
        }
        if(written < date_str_size - 1) {
            snprintf(date_str + written, date_str_size - written, "%02u", year % 100);
        }
    } else {
        // If the buffer size is 0, do nothing
    }
}

static void calculate_time(uint32_t timestamp, char* time_str, size_t time_str_size) {
    uint32_t seconds_in_day = timestamp % 86400;
    uint8_t hours = seconds_in_day / 3600;
    uint8_t minutes = (seconds_in_day % 3600) / 60;
    snprintf(time_str, time_str_size, "%02u:%02u", hours, minutes);
}

static bool smartrider_parse_data(SmartRiderData* sr_data, const MfClassicData* data) {
    if(data->type != MfClassicType1k) {
        FURI_LOG_E(TAG, "Invalid card type");
        return false;
    }

    static const uint8_t required_blocks[] = {14, 4, 5, 1, 52, 50, 0};
    for(size_t i = 0; i < COUNT_OF(required_blocks); i++) {
        if(required_blocks[i] >= MAX_BLOCKS ||
           !mf_classic_is_block_read(data, required_blocks[i])) {
            FURI_LOG_E(TAG, "Required block %d is not read or out of range", required_blocks[i]);
            return false;
        }
    }

    sr_data->balance = bit_lib_bytes_to_num_le(data->block[14].data + 7, 2);
    sr_data->issued_days = bit_lib_bytes_to_num_le(data->block[4].data + 16, 2);
    sr_data->expiry_days = bit_lib_bytes_to_num_le(data->block[4].data + 18, 2);
    sr_data->auto_load_threshold = bit_lib_bytes_to_num_le(data->block[4].data + 20, 2);
    sr_data->auto_load_value = bit_lib_bytes_to_num_le(data->block[4].data + 22, 2);
    sr_data->token = data->block[5].data[8];
    sr_data->purchase_cost = bit_lib_bytes_to_num_le(data->block[0].data + 14, 2);

    snprintf(
        sr_data->card_serial_number,
        sizeof(sr_data->card_serial_number),
        "%02X%02X%02X%02X%02X",
        data->block[1].data[6],
        data->block[1].data[7],
        data->block[1].data[8],
        data->block[1].data[9],
        data->block[1].data[10]);

    for(uint8_t block_number = 40; block_number <= 52 && sr_data->trip_count < MAX_TRIPS;
        block_number++) {
        if((block_number != 43 && block_number != 47 && block_number != 51) &&
           mf_classic_is_block_read(data, block_number) &&
           parse_trip_data(
               &data->block[block_number], &sr_data->trips[sr_data->trip_count], block_number)) {
            sr_data->trip_count++;
        }
    }

    // Sort trips by timestamp (descending order)
    for(uint8_t i = 0; i < sr_data->trip_count - 1; i++) {
        for(uint8_t j = 0; j < sr_data->trip_count - i - 1; j++) {
            if(sr_data->trips[j].timestamp < sr_data->trips[j + 1].timestamp) {
                TripData temp = sr_data->trips[j];
                sr_data->trips[j] = sr_data->trips[j + 1];
                sr_data->trips[j + 1] = temp;
            }
        }
    }

    return true;
}

/* Parse MIFARE Classic data and populate card view */
static bool
    smartrider_display_card_view(const MfClassicData* data, Metroflip* app, bool from_file) {
    SmartRiderData sr_data = {0};

    if(!smartrider_parse_data(&sr_data, data)) {
        return false;
    }

    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "SmartRider");

    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    /* Page: Overview */
    uint8_t p = metroflip_card_view_add_page(view, "Overview");

    snprintf(val, sizeof(val), "$%lu.%02lu", sr_data.balance / 100, sr_data.balance % 100);
    metroflip_card_view_add_field(view, p, "Balance", val, true);

    const char* concession = get_concession_type(sr_data.token);
    metroflip_card_view_add_field(
        view, p, "Concession", concession ? concession : "Unknown", false);

    // Build serial: prefix SR0 if starts with 00
    char serial_display[METROFLIP_CARD_VIEW_VALUE_LEN];
    char sn_copy[12];
    strncpy(sn_copy, sr_data.card_serial_number, sizeof(sn_copy) - 1);
    sn_copy[sizeof(sn_copy) - 1] = '\0';
    if(memcmp(sn_copy, "00", 2) == 0) {
        snprintf(serial_display, sizeof(serial_display), "SR0%s", sn_copy + 2);
    } else {
        snprintf(serial_display, sizeof(serial_display), "%s", sn_copy);
    }
    metroflip_card_view_add_field(view, p, "Serial", serial_display, false);

    /* Page: Details */
    p = metroflip_card_view_add_page(view, "Details");

    snprintf(
        val, sizeof(val), "$%u.%02u", sr_data.purchase_cost / 100, sr_data.purchase_cost % 100);
    metroflip_card_view_add_field(view, p, "Total Cost", val, false);

    snprintf(
        val,
        sizeof(val),
        "$%u.%02u",
        sr_data.auto_load_threshold / 100,
        sr_data.auto_load_threshold % 100);
    metroflip_card_view_add_field(view, p, "AL Threshold", val, false);

    snprintf(
        val, sizeof(val), "$%u.%02u", sr_data.auto_load_value / 100, sr_data.auto_load_value % 100);
    metroflip_card_view_add_field(view, p, "AL Value", val, false);

    /* Trip history pages (max 16 pages total, we used 2 already, so up to 14 trips but MAX_TRIPS=10) */
    for(uint8_t i = 0; i < sr_data.trip_count; i++) {
        char hdr[METROFLIP_CARD_VIEW_HEADER_LEN];
        snprintf(
            hdr,
            sizeof(hdr),
            "Trip %d - %s",
            i + 1,
            sr_data.trips[i].tap_on ? "Tap On" : "Tap Off");

        p = metroflip_card_view_add_page(view, hdr);
        if(p == UINT8_MAX) break;

        char date_str[9];
        calculate_date(sr_data.trips[i].timestamp, date_str, sizeof(date_str));
        char time_str[6];
        calculate_time(sr_data.trips[i].timestamp, time_str, sizeof(time_str));
        snprintf(val, sizeof(val), "%s %s", date_str, time_str);
        metroflip_card_view_add_field(view, p, "When", val, false);

        metroflip_card_view_add_field(view, p, "Route", sr_data.trips[i].route, false);

        uint32_t cost = sr_data.trips[i].cost;
        if(cost > 0) {
            snprintf(val, sizeof(val), "$%lu.%02lu", cost / 100, cost % 100);
            metroflip_card_view_add_field(view, p, "Cost", val, true);
        }

        snprintf(val, sizeof(val), "%u", sr_data.trips[i].journey_number);
        metroflip_card_view_add_field(view, p, "Journey", val, false);
    }

    /* Button configuration */
    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

// made with love by jay candel <3

static NfcCommand smartrider_poller_callback(NfcGenericEvent event, void* context) {
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

        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(app->nfc_device, &uid_len);
        /*-----------------All of this is to store a keyfile in a permanent way for the user to always access------------*/
        /*-----------------Open cache file (if exists)------------*/

        char uid_str[uid_len * 2 + 1];
        uid_to_string(uid, uid_len, uid_str, sizeof(uid_str));
        uint64_t smartrider_key_mask_a_required = 12299; // 11000000001011
        KeyfileManager manage = manage_keyfiles(
            uid_str, uid, uid_len, app->mfc_key_cache, smartrider_key_mask_a_required, 0);

        char card_type[] = "SmartRider";

        switch(manage) {
        case MISSING_KEYFILE: {
            // The widget copies the text, so the string can be freed right away
            FuriString* parsed_data = furi_string_alloc();
            handle_keyfile_case(app, "No keys found", "Missing keyfile", parsed_data, card_type);
            furi_string_free(parsed_data);
            command = NfcCommandStop;
            break;
        }

        case INCOMPLETE_KEYFILE: {
            FuriString* parsed_data = furi_string_alloc();
            handle_keyfile_case(
                app, "Incomplete keyfile", "incomplete keyfile", parsed_data, card_type);
            furi_string_free(parsed_data);
            command = NfcCommandStop;
            break;
        }

        case SUCCESSFUL:
            mf_classic_key_cache_load(app->mfc_key_cache, uid, uid_len);
            FURI_LOG_I(TAG, "success");
            break;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        FURI_LOG_I(TAG, "sec_num: %d", smartrider_sector_num);
        MfClassicKey key = {};
        MfClassicKeyType key_type = MfClassicKeyTypeA;
        if(mf_classic_key_cache_get_next_key(
               app->mfc_key_cache, &smartrider_sector_num, &key, &key_type)) {
            mfc_event->data->read_sector_request_data.sector_num = smartrider_sector_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
        } else {
            mfc_event->data->read_sector_request_data.key_provided = false;
        }
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

static void smartrider_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    mf_classic_key_cache_reset(app->mfc_key_cache);

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);

            if(!smartrider_display_card_view(mfc_data, app, true)) {
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
        Popup* popup = app->popup;
        popup_set_header(
            popup, "Scanning...\nApply card\nto the back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, smartrider_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool smartrider_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Read finished on the worker thread; build the card view here on
               the main/GUI thread. */
            metroflip_app_blink_stop(app);
            dolphin_deed(DolphinDeedNfcReadSuccess);
            const MfClassicData* mfc_data =
                nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
            if(!smartrider_display_card_view(mfc_data, app, false)) {
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

static void smartrider_on_exit(Metroflip* app) {
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
static const MetroflipPlugin smartrider_plugin = {
    .card_name = "SmartRider",
    .plugin_on_enter = smartrider_on_enter,
    .plugin_on_event = smartrider_on_event,
    .plugin_on_exit = smartrider_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor smartrider_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &smartrider_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* smartrider_plugin_ep(void) {
    return &smartrider_plugin_descriptor;
}
