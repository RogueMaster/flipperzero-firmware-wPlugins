/*
 * Parser for bip card (Georgia).
 *
 * Copyright 2023 Leptoptilos <leptoptilos@icloud.com>
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

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>
#include <locale/locale.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#define TAG "Metroflip:Scene:Bip"

#define BIP_CARD_ID_SECTOR_NUMBER          (0)
#define BIP_BALANCE_SECTOR_NUMBER          (8)
#define BIP_TRIP_TIME_WINDOW_SECTOR_NUMBER (5)
#define BIP_LAST_TOP_UPS_SECTOR_NUMBER     (10)
#define BIP_TRIPS_INFO_SECTOR_NUMBER       (11)

const MfClassicKeyPair bip_1k_keys[16] = {
    {.a = 0x3a42f33af429, .b = 0x1fc235ac1309},
    {.a = 0x6338a371c0ed, .b = 0x243f160918d1},
    {.a = 0xf124c2578ad0, .b = 0x9afc42372af1},
    {.a = 0x32ac3b90ac13, .b = 0x682d401abb09},
    {.a = 0x4ad1e273eaf1, .b = 0x067db45454a9},
    {.a = 0xe2c42591368a, .b = 0x15fc4c7613fe},
    {.a = 0x2a3c347a1200, .b = 0x68d30288910a},
    {.a = 0x16f3d5ab1139, .b = 0xf59a36a2546d},
    {.a = 0x937a4fff3011, .b = 0x64e3c10394c2},
    {.a = 0x35c3d2caee88, .b = 0xb736412614af},
    {.a = 0x693143f10368, .b = 0x324f5df65310},
    {.a = 0xa3f97428dd01, .b = 0x643fb6de2217},
    {.a = 0x63f17a449af0, .b = 0x82f435dedf01},
    {.a = 0xc4652c54261c, .b = 0x0263de1278f3},
    {.a = 0xd49e2826664f, .b = 0x51284c3686a6},
    {.a = 0x3df14c8000a1, .b = 0x6a470d54127c},
};

typedef struct {
    DateTime datetime;
    uint16_t amount;
} BipTransaction;

static void bip_parse_datetime(const MfClassicBlock* block, DateTime* parsed_data) {
    furi_assert(block);
    furi_assert(parsed_data);

    parsed_data->day = (((block->data[1] << 8) + block->data[0]) >> 6) & 0x1f;
    parsed_data->month = (((block->data[1] << 8) + block->data[0]) >> 11) & 0xf;
    parsed_data->year = 2000 + ((((block->data[2] << 8) + block->data[1]) >> 7) & 0x1f);
    parsed_data->hour = (((block->data[3] << 8) + block->data[2]) >> 4) & 0x1f;
    parsed_data->minute = (((block->data[3] << 8) + block->data[2]) >> 9) & 0x3f;
    parsed_data->second = (((block->data[4] << 8) + block->data[3]) >> 7) & 0x3f;
}

static void bip_print_datetime(const DateTime* datetime, FuriString* str) {
    furi_assert(datetime);
    furi_assert(str);

    LocaleDateFormat date_format = locale_get_date_format();
    const char* separator = (date_format == LocaleDateFormatDMY) ? "." : "/";

    FuriString* date_str = furi_string_alloc();
    locale_format_date(date_str, datetime, date_format, separator);

    FuriString* time_str = furi_string_alloc();
    locale_format_time(time_str, datetime, locale_get_time_format(), true);

    furi_string_cat_printf(
        str, "%s %s", furi_string_get_cstr(date_str), furi_string_get_cstr(time_str));

    furi_string_free(date_str);
    furi_string_free(time_str);
}

static int datetime_cmp(const DateTime* dt_1, const DateTime* dt_2) {
    furi_assert(dt_1);
    furi_assert(dt_2);

    if(dt_1->year != dt_2->year) {
        return dt_1->year - dt_2->year;
    }
    if(dt_1->month != dt_2->month) {
        return dt_1->month - dt_2->month;
    }
    if(dt_1->day != dt_2->day) {
        return dt_1->day - dt_2->day;
    }
    if(dt_1->hour != dt_2->hour) {
        return dt_1->hour - dt_2->hour;
    }
    if(dt_1->minute != dt_2->minute) {
        return dt_1->minute - dt_2->minute;
    }
    if(dt_1->second != dt_2->second) {
        return dt_1->second - dt_2->second;
    }
    return 0;
}

static bool is_bip_block_empty(const MfClassicBlock* block) {
    furi_assert(block);
    // check if all but last byte are zero (last is checksum)
    for(size_t i = 0; i < sizeof(block->data) - 1; i++) {
        if(block->data[i] != 0) {
            return false;
        }
    }
    return true;
}

static bool bip_parse(FuriString* parsed_data, const MfClassicData* data) {
    furi_assert(parsed_data);

    struct {
        uint32_t card_id;
        uint16_t balance;
        uint16_t flags;
        DateTime trip_time_window;
        BipTransaction top_ups[3];
        BipTransaction charges[3];
    } bip_data = {0};

    bool parsed = false;

    do {
        // verify sector 0 key A
        MfClassicSectorTrailer* sec_tr = mf_classic_get_sector_trailer_by_sector(data, 0);

        if(data->type != MfClassicType1k) break;

        uint64_t key = bit_lib_bytes_to_num_be(sec_tr->key_a.data, 6);
        if(key != bip_1k_keys[0].a) {
            break;
        }
        // Get Card ID, little-endian 4 bytes at sector 0 block 1, bytes 4-7
        const uint8_t card_id_start_block_num =
            mf_classic_get_first_block_num_of_sector(BIP_CARD_ID_SECTOR_NUMBER);
        const uint8_t* block_start_ptr = &data->block[card_id_start_block_num + 1].data[0];

        bip_data.card_id = bit_lib_bytes_to_num_le(block_start_ptr + 4, 4);

        // Get balance, little-endian 2 bytes at sector 8 block 1, bytes 0-1
        const uint8_t balance_start_block_num =
            mf_classic_get_first_block_num_of_sector(BIP_BALANCE_SECTOR_NUMBER);
        block_start_ptr = &data->block[balance_start_block_num + 1].data[0];

        bip_data.balance = bit_lib_bytes_to_num_le(block_start_ptr, 2);

        // Get balance flags (negative balance, etc.), little-endian 2 bytes at sector 8 block 1, bytes 2-3
        bip_data.flags = bit_lib_bytes_to_num_le(block_start_ptr + 2, 2);

        // Get trip time window, proprietary format, at sector 5 block 1, bytes 0-7
        const uint8_t trip_time_window_start_block_num =
            mf_classic_get_first_block_num_of_sector(BIP_TRIP_TIME_WINDOW_SECTOR_NUMBER);
        const MfClassicBlock* trip_window_block_ptr =
            &data->block[trip_time_window_start_block_num + 1];

        bip_parse_datetime(trip_window_block_ptr, &bip_data.trip_time_window);

        // Last 3 top-ups: sector 10, ring-buffer of 3 blocks, timestamp in bytes 0-7, amount in bytes 9-10
        const uint8_t top_ups_start_block_num =
            mf_classic_get_first_block_num_of_sector(BIP_LAST_TOP_UPS_SECTOR_NUMBER);
        for(size_t i = 0; i < 3; i++) {
            const MfClassicBlock* block = &data->block[top_ups_start_block_num + i];

            if(is_bip_block_empty(block)) continue;

            BipTransaction* top_up = &bip_data.top_ups[i];
            bip_parse_datetime(block, &top_up->datetime);

            top_up->amount = bit_lib_bytes_to_num_le(&block->data[9], 2) >> 2;
        }

        // Last 3 charges (i.e. trips), sector 11, ring-buffer of 3 blocks, timestamp in bytes 0-7, amount in bytes 10-11
        const uint8_t trips_start_block_num =
            mf_classic_get_first_block_num_of_sector(BIP_TRIPS_INFO_SECTOR_NUMBER);
        for(size_t i = 0; i < 3; i++) {
            const MfClassicBlock* block = &data->block[trips_start_block_num + i];

            if(is_bip_block_empty(block)) continue;

            BipTransaction* charge = &bip_data.charges[i];
            bip_parse_datetime(block, &charge->datetime);

            charge->amount = bit_lib_bytes_to_num_le(&block->data[10], 2) >> 2;
        }

        // All data is now parsed and stored in bip_data, now print it

        // Print basic info
        furi_string_printf(
            parsed_data,
            "\e#Tarjeta Bip!\n"
            "Card Number: %lu\n"
            "Balance: $%hu (flags %hu)\n"
            "Current Trip Window Ends:\n  @",
            bip_data.card_id,
            bip_data.balance,
            bip_data.flags);

        bip_print_datetime(&bip_data.trip_time_window, parsed_data);

        // Find newest top-up
        size_t newest_top_up = 0;
        for(size_t i = 1; i < 3; i++) {
            const DateTime* newest = &bip_data.top_ups[newest_top_up].datetime;
            const DateTime* current = &bip_data.top_ups[i].datetime;
            if(datetime_cmp(current, newest) > 0) {
                newest_top_up = i;
            }
        }

        // Print top-ups, newest first
        furi_string_cat_printf(parsed_data, "\n\e#Last Top-ups");
        for(size_t i = 0; i < 3; i++) {
            const BipTransaction* top_up = &bip_data.top_ups[(3u + newest_top_up - i) % 3];
            furi_string_cat_printf(parsed_data, "\n+$%d\n  @", top_up->amount);
            bip_print_datetime(&top_up->datetime, parsed_data);
        }

        // Find newest charge
        size_t newest_charge = 0;
        for(size_t i = 1; i < 3; i++) {
            const DateTime* newest = &bip_data.charges[newest_charge].datetime;
            const DateTime* current = &bip_data.charges[i].datetime;
            if(datetime_cmp(current, newest) > 0) {
                newest_charge = i;
            }
        }

        // Print charges
        furi_string_cat_printf(parsed_data, "\n\e#Last Charges (Trips)");
        for(size_t i = 0; i < 3; i++) {
            const BipTransaction* charge = &bip_data.charges[(3u + newest_charge - i) % 3];
            furi_string_cat_printf(parsed_data, "\n-$%d\n  @", charge->amount);
            bip_print_datetime(&charge->datetime, parsed_data);
        }

        parsed = true;
    } while(false);

    return parsed;
}

static NfcCommand bip_poller_callback(NfcGenericEvent event, void* context) {
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
        bit_lib_num_to_bytes_be(bip_1k_keys[app->sec_num].a, COUNT_OF(key.data), key.data);

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
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        FuriString* parsed_data = furi_string_alloc();
        Widget* widget = app->widget;

        dolphin_deed(DolphinDeedNfcReadSuccess);
        furi_string_reset(app->text_box_store);
        if(!bip_parse(parsed_data, mfc_data)) {
            furi_string_reset(app->text_box_store);
            FURI_LOG_I(TAG, "Unknown card type");
            furi_string_printf(parsed_data, "\e#Unknown card\n");
        }
        metroflip_app_blink_stop(app);
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);

        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

static void bip_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!bip_parse(parsed_data, mfc_data)) {
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
            mf_classic_free(mfc_data);
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
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, bip_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool bip_on_event(Metroflip* app, SceneManagerEvent event) {
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

static void bip_on_exit(Metroflip* app) {
    widget_reset(app->widget);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}

/* Actual implementation of app<>plugin interface */
static const MetroflipPlugin bip_plugin = {
    .card_name = "Bip",
    .plugin_on_enter = bip_on_enter,
    .plugin_on_event = bip_on_event,
    .plugin_on_exit = bip_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor bip_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &bip_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* bip_plugin_ep(void) {
    return &bip_plugin_descriptor;
}
