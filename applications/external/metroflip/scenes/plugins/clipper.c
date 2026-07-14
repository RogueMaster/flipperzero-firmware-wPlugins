/*
 * clipper.c - Parser for Clipper cards (San Francisco, California).
 *
 * Based on research, some of which dates to 2007!
 *
 * Copyright 2024 Jeremy Cooper <jeremy.gthb@baymoo.org>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <flipper_application.h>
#include "../../metroflip_i.h"
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>

#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>

#include <bit_lib.h>
#include <datetime.h>
#include <locale/locale.h>
#include <inttypes.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:Clipper"

//
// Table of application ids observed in the wild, and their sources.
//
static const struct {
    const MfDesfireApplicationId app;
    const char* type;
} clipper_types[] = {
    // Application advertised on classic, plastic cards.
    {.app = {.data = {0x90, 0x11, 0xf2}}, .type = "Card"},
    // Application advertised on a mobile device.
    {.app = {.data = {0x91, 0x11, 0xf2}}, .type = "Mobile Device"},
};
static const size_t kNumCardTypes = sizeof(clipper_types) / sizeof(clipper_types[0]);

struct IdMapping_struct {
    uint16_t id;
    const char* name;
};
typedef struct IdMapping_struct IdMapping;

#define COUNT(_array) sizeof(_array) / sizeof(_array[0])

//
// Known transportation agencies and their identifiers.
//
static const IdMapping agency_names[] = {
    {.id = 0x0001, .name = "AC Transit"},
    {.id = 0x0004, .name = "BART"},
    {.id = 0x0006, .name = "Caltrain"},
    {.id = 0x0008, .name = "CCTA"},
    {.id = 0x000b, .name = "GGT"},
    {.id = 0x000f, .name = "SamTrans"},
    {.id = 0x0011, .name = "VTA"},
    {.id = 0x0012, .name = "Muni"},
    {.id = 0x0019, .name = "GG Ferry"},
    {.id = 0x001b, .name = "SF Bay Ferry"},
};
static const size_t kNumAgencies = COUNT(agency_names);

//
// Known station names for various agencies.
//
static const IdMapping bart_zones[] = {
    {.id = 0x0001, .name = "Colma"},           {.id = 0x0002, .name = "Daly City"},
    {.id = 0x0003, .name = "Balboa Park"},     {.id = 0x0004, .name = "Glen Park"},
    {.id = 0x0005, .name = "24th St Mission"}, {.id = 0x0006, .name = "16th St Mission"},
    {.id = 0x0007, .name = "Civic Ctr"},       {.id = 0x0008, .name = "Powell St"},
    {.id = 0x0009, .name = "Montgomery St"},   {.id = 0x000a, .name = "Embarcadero"},
    {.id = 0x000b, .name = "West Oakland"},    {.id = 0x000c, .name = "12th St Oakland"},
    {.id = 0x000d, .name = "19th St Oakland"}, {.id = 0x000e, .name = "MacArthur"},
    {.id = 0x000f, .name = "Rockridge"},       {.id = 0x0010, .name = "Orinda"},
    {.id = 0x0011, .name = "Lafayette"},       {.id = 0x0012, .name = "Walnut Creek"},
    {.id = 0x0013, .name = "Pleasant Hill"},   {.id = 0x0014, .name = "Concord"},
    {.id = 0x0015, .name = "N Concord"},       {.id = 0x0016, .name = "Pittsburg/BP"},
    {.id = 0x0017, .name = "Ashby"},           {.id = 0x0018, .name = "Downtown Berk"},
    {.id = 0x0019, .name = "North Berkeley"},  {.id = 0x001a, .name = "El Cerrito Plz"},
    {.id = 0x001b, .name = "El Cerrito DN"},   {.id = 0x001c, .name = "Richmond"},
    {.id = 0x001d, .name = "Lake Merrit"},     {.id = 0x001e, .name = "Fruitvale"},
    {.id = 0x001f, .name = "Coliseum"},        {.id = 0x0021, .name = "San Leandro"},
    {.id = 0x0022, .name = "Hayward"},         {.id = 0x0023, .name = "South Hayward"},
    {.id = 0x0024, .name = "Union City"},      {.id = 0x0025, .name = "Fremont"},
    {.id = 0x0026, .name = "Castro Valley"},   {.id = 0x0027, .name = "Dublin/Plsntn"},
    {.id = 0x0028, .name = "S San Francisco"}, {.id = 0x0029, .name = "San Bruno"},
    {.id = 0x002a, .name = "SFO Airport"},     {.id = 0x002b, .name = "Millbrae"},
    {.id = 0x002c, .name = "W Dublin/Plsn"},   {.id = 0x002d, .name = "OAK Airport"},
    {.id = 0x002e, .name = "Warm Springs"},    {.id = 0x002f, .name = "Milpitas"},
    {.id = 0x0030, .name = "Berryessa/NSJ"},
};
static const size_t kNumBARTZones = COUNT(bart_zones);

static const IdMapping muni_zones[] = {
    {.id = 0x0000, .name = "City Street"},
    {.id = 0x0005, .name = "Embarcadero"},
    {.id = 0x0006, .name = "Montgomery"},
    {.id = 0x0007, .name = "Powell"},
    {.id = 0x0008, .name = "Civic Center"},
    {.id = 0x0009, .name = "Van Ness"},
    {.id = 0x000a, .name = "Church"},
    {.id = 0x000b, .name = "Castro"},
    {.id = 0x000c, .name = "Forest Hill"},
    {.id = 0x000d, .name = "West Portal"},
};
static const size_t kNumMUNIZones = COUNT(muni_zones);

static const IdMapping actransit_zones[] = {
    {.id = 0x0000, .name = "City Street"},
};
static const size_t kNumACTransitZones = COUNT(actransit_zones);

static const IdMapping caltrain_zones[] = {
    {.id = 0x0001, .name = "Zone 1"},
    {.id = 0x0002, .name = "Zone 2"},
    {.id = 0x0003, .name = "Zone 3"},
    {.id = 0x0004, .name = "Zone 4"},
    {.id = 0x0005, .name = "Zone 5"},
    {.id = 0x0006, .name = "Zone 6"},
};
static const size_t kNumCaltrainZones = COUNT(caltrain_zones);

static const struct {
    uint16_t agency_id;
    const IdMapping* zone_map;
    size_t zone_count;
} agency_zone_map[] = {
    {.agency_id = 0x0001, .zone_map = actransit_zones, .zone_count = kNumACTransitZones},
    {.agency_id = 0x0004, .zone_map = bart_zones, .zone_count = kNumBARTZones},
    {.agency_id = 0x0006, .zone_map = caltrain_zones, .zone_count = kNumCaltrainZones},
    {.agency_id = 0x0012, .zone_map = muni_zones, .zone_count = kNumMUNIZones}};
static const size_t kNumAgencyZoneMaps = COUNT(agency_zone_map);

// File ids of important files on the card.
static const MfDesfireFileId clipper_ecash_file_id = 2;
static const MfDesfireFileId clipper_histidx_file_id = 6;
static const MfDesfireFileId clipper_identity_file_id = 8;
static const MfDesfireFileId clipper_history_file_id = 14;

struct ClipperCardInfo_struct {
    uint32_t serial_number;
    uint16_t counter;
    uint16_t last_txn_id;
    uint32_t last_updated_tm_1900;
    uint16_t last_terminal_id;
    int16_t balance_cents;
};
typedef struct ClipperCardInfo_struct ClipperCardInfo;

// Unmarshal helpers
static inline uint32_t get_u32be(const uint8_t* field) {
    return bit_lib_bytes_to_num_be(field, 4);
}

static uint16_t get_u16be(const uint8_t* field) {
    return bit_lib_bytes_to_num_be(field, 2);
}

static int16_t get_i16be(const uint8_t* field) {
    uint16_t raw = get_u16be(field);
    if(raw > 0x7fff)
        return -((uint32_t)0x10000 - raw);
    else
        return raw;
}

static bool get_map_item(uint16_t id, const IdMapping* map, size_t sz, const char** out) {
    for(size_t i = 0; i < sz; i++) {
        if(map[i].id == id) {
            *out = map[i].name;
            return true;
        }
    }
    return false;
}

static bool get_agency_zone_name(uint16_t agency_id, uint16_t zone_id, const char** out) {
    for(size_t i = 0; i < kNumAgencyZoneMaps; i++) {
        if(agency_zone_map[i].agency_id == agency_id) {
            return get_map_item(
                zone_id, agency_zone_map[i].zone_map, agency_zone_map[i].zone_count, out);
        }
    }
    return false;
}

static void
    decode_usd(int16_t amount_cents, bool* out_is_negative, int16_t* out_usd, uint16_t* out_cents) {
    *out_usd = amount_cents / 100;
    if(amount_cents >= 0) {
        *out_is_negative = false;
        *out_cents = amount_cents % 100;
    } else {
        *out_is_negative = true;
        *out_cents = (amount_cents * -1) % 100;
    }
}

static bool get_file_contents(
    const MfDesfireApplication* app,
    const MfDesfireFileId* id,
    MfDesfireFileType type,
    size_t min_size,
    const uint8_t** out) {
    const MfDesfireFileSettings* settings = mf_desfire_get_file_settings(app, id);
    if(settings == NULL) return false;
    if(settings->type != type) return false;
    const MfDesfireFileData* file_data = mf_desfire_get_file_data(app, id);
    if(file_data == NULL) return false;
    if(simple_array_get_count(file_data->data) < min_size) return false;
    *out = simple_array_cget_data(file_data->data);
    return true;
}

static bool decode_id_file(const uint8_t* ef8_data, ClipperCardInfo* info) {
    info->serial_number = bit_lib_bytes_to_num_be(&ef8_data[1], 4);
    return true;
}

static bool decode_cash_file(const uint8_t* ef2_data, ClipperCardInfo* info) {
    info->counter = get_u16be(&ef2_data[2]);
    info->last_updated_tm_1900 = get_u32be(&ef2_data[4]);
    info->last_terminal_id = get_u16be(&ef2_data[8]);
    info->last_txn_id = get_u16be(&ef2_data[0x10]);
    info->balance_cents = get_i16be(&ef2_data[0x12]);
    return true;
}

/* Format a 1900-epoch timestamp into a short string for card view fields */
static void format_ts_1900(uint32_t ts, char* out, size_t len) {
    DateTime tm;
    ts -= 2208988800;
    datetime_timestamp_to_datetime(ts, &tm);
    FuriString* d = furi_string_alloc();
    locale_format_date(d, &tm, locale_get_date_format(), "-");
    FuriString* t = furi_string_alloc();
    locale_format_time(t, &tm, locale_get_time_format(), true);
    snprintf(out, len, "%s %s (UTC)", furi_string_get_cstr(d), furi_string_get_cstr(t));
    furi_string_free(d);
    furi_string_free(t);
}

/* Add a ride record as a card-view page */
static bool clipper_add_ride_page(View* view, const uint8_t* record, uint8_t ride_num) {
    if(record[0] != 0x10) return false;

    uint16_t agency_id = get_u16be(&record[2]);
    if(agency_id == 0) return false;

    const char* agency_name;
    if(!get_map_item(agency_id, agency_names, kNumAgencies, &agency_name)) agency_name = "Unknown";

    uint16_t vehicle_id = get_u16be(&record[0x0a]);

    int16_t fare_raw = get_i16be(&record[6]);
    bool _neg;
    int16_t fare_usd;
    uint16_t fare_cents;
    decode_usd(fare_raw, &_neg, &fare_usd, &fare_cents);

    uint32_t time_on = get_u32be(&record[0x0c]);
    uint32_t time_off = get_u32be(&record[0x10]);
    uint16_t zone_on_id = get_u16be(&record[0x14]);
    uint16_t zone_off_id = get_u16be(&record[0x16]);

    const char* zone_on;
    const char* zone_off;
    if(!get_agency_zone_name(agency_id, zone_on_id, &zone_on)) zone_on = "Unknown";
    if(!get_agency_zone_name(agency_id, zone_off_id, &zone_off)) zone_off = "Unknown";

    char hdr[METROFLIP_CARD_VIEW_HEADER_LEN];
    snprintf(hdr, sizeof(hdr), "Ride %d - %s", ride_num, agency_name);

    uint8_t p = metroflip_card_view_add_page(view, hdr);
    if(p == UINT8_MAX) return false;

    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    format_ts_1900(time_on, val, sizeof(val));
    metroflip_card_view_add_field(view, p, "When", val, false);

    snprintf(val, sizeof(val), "$%d.%02u", fare_usd, fare_cents);
    metroflip_card_view_add_field(view, p, "Fare", val, true);

    snprintf(val, sizeof(val), "%s (%04x)", agency_name, agency_id);
    metroflip_card_view_add_field(view, p, "Agency", val, false);

    snprintf(val, sizeof(val), "%s (%04x)", zone_on, zone_on_id);
    metroflip_card_view_add_field(view, p, "Board", val, false);

    if(vehicle_id != 0) {
        snprintf(val, sizeof(val), "%u", vehicle_id);
        metroflip_card_view_add_field(view, p, "Vehicle", val, false);
    }

    if(time_off != 0) {
        snprintf(val, sizeof(val), "%s (%04x)", zone_off, zone_off_id);
        metroflip_card_view_add_field(view, p, "Exit", val, false);

        format_ts_1900(time_off, val, sizeof(val));
        metroflip_card_view_add_field(view, p, "Exit time", val, false);
    }

    return true;
}

/* Parse DESFire data and populate card view */
static bool clipper_display_card_view(const MfDesfireData* data, Metroflip* app, bool from_file) {
    const MfDesfireApplication* mf_app = NULL;
    const char* device_desc = NULL;

    for(size_t i = 0; i < kNumCardTypes; i++) {
        mf_app = mf_desfire_get_application(data, &clipper_types[i].app);
        device_desc = clipper_types[i].type;
        if(mf_app != NULL) break;
    }
    if(mf_app == NULL) return false;

    ClipperCardInfo info;
    const uint8_t* id_data;
    if(!get_file_contents(
           mf_app, &clipper_identity_file_id, MfDesfireFileTypeStandard, 5, &id_data))
        return false;
    if(!decode_id_file(id_data, &info)) return false;

    const uint8_t* cash_data;
    if(!get_file_contents(mf_app, &clipper_ecash_file_id, MfDesfireFileTypeBackup, 32, &cash_data))
        return false;
    if(!decode_cash_file(cash_data, &info)) return false;

    int16_t bal_usd;
    uint16_t bal_cents;
    bool _neg;
    decode_usd(info.balance_cents, &_neg, &bal_usd, &bal_cents);

    /* Allocate card view */
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "Clipper");

    /* Page: Overview */
    uint8_t p = metroflip_card_view_add_page(view, "Overview");
    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    snprintf(val, sizeof(val), "%" PRIu32, info.serial_number);
    metroflip_card_view_add_field(view, p, "Serial", val, false);

    snprintf(val, sizeof(val), "$%d.%02u", bal_usd, bal_cents);
    metroflip_card_view_add_field(view, p, "Balance", val, true);

    metroflip_card_view_add_field(view, p, "Type", device_desc, false);

    if(info.last_updated_tm_1900 != 0) {
        format_ts_1900(info.last_updated_tm_1900, val, sizeof(val));
        metroflip_card_view_add_field(view, p, "Updated", val, false);
    } else {
        metroflip_card_view_add_field(view, p, "Updated", "Never", false);
    }

    /* Page: Details */
    p = metroflip_card_view_add_page(view, "Details");

    snprintf(val, sizeof(val), "0x%04X", info.last_terminal_id);
    metroflip_card_view_add_field(view, p, "Terminal", val, false);

    snprintf(val, sizeof(val), "%u", info.last_txn_id);
    metroflip_card_view_add_field(view, p, "Txn ID", val, false);

    snprintf(val, sizeof(val), "%u", info.counter);
    metroflip_card_view_add_field(view, p, "Counter", val, false);

    /* Ride history pages */
    const uint8_t *history_index, *history;
    if(get_file_contents(
           mf_app, &clipper_histidx_file_id, MfDesfireFileTypeBackup, 16, &history_index) &&
       get_file_contents(
           mf_app, &clipper_history_file_id, MfDesfireFileTypeStandard, 512, &history)) {
        static const size_t kRideSize = 0x20;
        uint8_t ride_num = 1;
        for(size_t i = 0; i < 16; i++) {
            uint8_t rec = history_index[i];
            if(rec == 0xff) break;
            size_t off = rec * kRideSize;
            if(off + kRideSize > 512) break;
            if(!clipper_add_ride_page(view, &history[off], ride_num)) continue;
            ride_num++;
        }
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

static NfcCommand clipper_poller_callback(NfcGenericEvent event, void* context) {
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

static void clipper_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfDesfireData* data = mf_desfire_alloc();
            mf_desfire_load(data, ff, 2);

            if(!clipper_display_card_view(data, app, true)) {
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
        nfc_poller_start(app->poller, clipper_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool clipper_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Read finished on the worker thread; build the card view here on
               the main/GUI thread. */
            metroflip_app_blink_stop(app);
            const MfDesfireData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfDesfire);
            if(!clipper_display_card_view(data, app, false)) {
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

static void clipper_on_exit(Metroflip* app) {
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
static const MetroflipPlugin clipper_plugin = {
    .card_name = "Clipper",
    .plugin_on_enter = clipper_on_enter,
    .plugin_on_event = clipper_on_event,
    .plugin_on_exit = clipper_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor clipper_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &clipper_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* clipper_plugin_ep(void) {
    return &clipper_plugin_descriptor;
}
