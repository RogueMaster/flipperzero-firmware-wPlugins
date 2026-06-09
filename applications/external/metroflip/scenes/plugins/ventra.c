// Parser for CTA Ventra Ultralight cards
// Made by @hazardousvoltage
// Based on my own research, with...
// Credit to https://www.lenrek.net/experiments/compass-tickets/ & MetroDroid project for underlying info
//
// This parser can decode the paper single-use and single/multi-day paper passes using Ultralight EV1
// The plastic cards are DESFire and fully locked down, not much useful info extractable
// TODO:
// - Sort the duplicate/rare ticket types
// - Database of stop IDs for trains?  Buses there's just too damn many, but you can find them here:
//   https://data.cityofchicago.org/Transportation/CTA-Bus-Stops-kml/84eu-buny/about_data
// - Generalize to handle all known Cubic Nextfare Ultralight systems?  Anyone wants to send me specimen dumps, hit me up on Discord.
//
// Ported to Metroflip by @hazardousvoltage 2026-06-08; Original marked as deprecated

#include <flipper_application.h>
#include "../../metroflip_i.h"

#include <dolphin/dolphin.h>
#include <furi_hal.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>

#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"
#include "../ventra.h"

#define TAG "Metroflip:Ventra"

typedef struct {
    DateTime hard_expiration;
    DateTime validity_expiration;
    uint8_t highest_sequence;
    uint8_t current_block;
    uint8_t minutes_active;
    bool is_pass;
} VentraParseContext;

static bool ventra_date_valid(DateTime* date) {
    if(date->year < 2000 || date->year > 2099 || date->month == 0 || date->month > 12 ||
       date->day == 0) {
        return false;
    }

    const uint8_t days_in_month =
        datetime_get_days_per_month(datetime_is_leap_year(date->year), date->month);
    if(date->day > days_in_month || date->hour > 23 || date->minute > 59) return false;

    date->weekday = 1;
    return true;
}

static bool ventra_shift_date(const DateTime* source, uint8_t days, DateTime* result) {
    DateTime source_copy = *source;
    if(!ventra_date_valid(&source_copy)) return false;

    const uint32_t timestamp = datetime_datetime_to_timestamp(&source_copy);
    const uint32_t delta = (uint32_t)days * 86400U;
    if(timestamp < delta) return false;

    datetime_timestamp_to_datetime(timestamp - delta, result);
    return true;
}

static bool ventra_is_expired(VentraParseContext* context) {
    DateTime hard_expiration = context->hard_expiration;
    DateTime validity_expiration = context->validity_expiration;
    if(!ventra_date_valid(&hard_expiration) || !ventra_date_valid(&validity_expiration)) {
        return false;
    }

    const uint32_t now = furi_hal_rtc_get_timestamp();
    return now >= datetime_datetime_to_timestamp(&hard_expiration) ||
           now > datetime_datetime_to_timestamp(&validity_expiration);
}

static bool ventra_parse_transaction(
    const MfUltralightData* data,
    uint8_t block,
    VentraParseContext* context,
    FuriString* output) {
    uint16_t packed_time = data->page[block].data[0] | ((uint16_t)data->page[block].data[1] << 8);
    const uint8_t transaction_type = packed_time & 0x1F;
    packed_time >>= 5;

    const uint8_t day_delta = data->page[block].data[2];
    const uint32_t work = data->page[block + 1].data[0] |
                          ((uint32_t)data->page[block + 1].data[1] << 8) |
                          ((uint32_t)data->page[block + 1].data[2] << 16);
    const uint8_t sequence = work & 0x7F;
    const uint16_t expiration_time = (work >> 7) & 0x7FF;
    const uint8_t expiration_day = data->page[block + 2].data[0];
    const uint16_t locus = data->page[block + 2].data[1] |
                           ((uint16_t)data->page[block + 2].data[2] << 8);
    const uint8_t line = data->page[block + 2].data[3];

    DateTime transaction_date = {0};
    if(!ventra_shift_date(&context->hard_expiration, day_delta, &transaction_date)) return false;

    transaction_date.hour = packed_time / 60;
    transaction_date.minute = packed_time % 60;
    if(!ventra_date_valid(&transaction_date)) return false;

    if(sequence == 0) {
        furi_string_printf(output, "-- EMPTY --");
        return true;
    }

    if(sequence > context->highest_sequence) {
        context->highest_sequence = sequence;
        context->current_block = block;
        context->minutes_active = data->page[block + 1].data[3];

        if(transaction_type == 6) {
            if(context->is_pass) {
                DateTime validity = {0};
                if(!ventra_shift_date(&context->hard_expiration, expiration_day, &validity)) {
                    return false;
                }
                validity.hour = expiration_time / 60;
                validity.minute = expiration_time % 60;
                if(!ventra_date_valid(&validity)) return false;
                context->validity_expiration = validity;
            } else if(context->minutes_active <= 120) {
                uint32_t validity_timestamp = datetime_datetime_to_timestamp(&transaction_date);
                validity_timestamp += (uint32_t)(120 - context->minutes_active) * 60U;
                datetime_timestamp_to_datetime(validity_timestamp, &context->validity_expiration);
            }
        }
    }

    switch(line) {
    case 0:
        furi_string_printf(output, "Purchase %04X", locus);
        break;
    case 1:
        furi_string_printf(output, "Train %04X", locus);
        break;
    case 2:
        furi_string_printf(output, "Bus %5u", locus);
        break;
    default:
        furi_string_printf(output, "Unknown %04X", locus);
        break;
    }

    furi_string_cat_printf(
        output,
        "\n  %04u-%02u-%02u %02u:%02u\n",
        transaction_date.year,
        transaction_date.month,
        transaction_date.day,
        transaction_date.hour,
        transaction_date.minute);

    return true;
}

static bool ventra_parse(FuriString* parsed_data, const MfUltralightData* data) {
    furi_assert(parsed_data);
    furi_assert(data);

    if(!metroflip_ventra_detect(data)) return false;

    VentraParseContext context = {0};
    FuriString* product = furi_string_alloc();
    FuriString* transaction_1 = furi_string_alloc();
    FuriString* transaction_2 = furi_string_alloc();
    bool parsed = false;

    do {
        const uint8_t product_code = data->page[5].data[2];
        switch(product_code) {
        case 2:
        case 0x1F:
            furi_string_set_str(product, "Single");
            break;
        case 3:
        case 0x3F:
            context.is_pass = true;
            furi_string_set_str(product, "1-Day");
            break;
        case 4:
            context.is_pass = true;
            furi_string_set_str(product, "3-Day");
            break;
        case 5:
        case 0x5F:
            context.is_pass = true;
            furi_string_set_str(product, "7-Day");
            break;
        default:
            context.is_pass = true;
            furi_string_printf(product, "0x%02X", product_code);
            break;
        }

        uint16_t encoded_date = data->page[4].data[3] | ((uint16_t)data->page[5].data[0] << 8);
        context.hard_expiration.day = encoded_date & 0x1F;
        context.hard_expiration.month = (encoded_date >> 5) & 0x0F;
        context.hard_expiration.year = 2000 + (encoded_date >> 9);
        if(!ventra_date_valid(&context.hard_expiration)) break;
        context.validity_expiration = context.hard_expiration;

        if(!ventra_parse_transaction(data, 8, &context, transaction_1) ||
           !ventra_parse_transaction(data, 12, &context, transaction_2)) {
            break;
        }

        uint8_t card_state = context.highest_sequence > 1 ? 2 : 1;
        uint8_t rides_left = 0;
        if(!context.is_pass) {
            switch(data->page[3].data[0]) {
            case 0:
                rides_left = 3;
                break;
            case 2:
                card_state = 2;
                rides_left = 2;
                break;
            case 6:
                card_state = 2;
                rides_left = 1;
                break;
            case 0x0E:
            case 0x7E:
                card_state = 3;
                break;
            default:
                card_state = 0;
                break;
            }
        }

        if(ventra_is_expired(&context)) {
            card_state = 4;
            rides_left = 0;
        }

        static const char* const card_states[] = {"???", "NEW", "ACT", "USED", "EXP"};
        furi_string_printf(
            parsed_data,
            "\e#Ventra %s (%s)\n",
            furi_string_get_cstr(product),
            card_states[card_state]);
        furi_string_cat_printf(
            parsed_data,
            "Exp: %04u-%02u-%02u %02u:%02u\n",
            context.validity_expiration.year,
            context.validity_expiration.month,
            context.validity_expiration.day,
            context.validity_expiration.hour,
            context.validity_expiration.minute);
        furi_string_cat_printf(
            parsed_data,
            "Hard Expiry: %04u-%02u-%02u\n",
            context.hard_expiration.year,
            context.hard_expiration.month,
            context.hard_expiration.day);
        if(rides_left) {
            furi_string_cat_printf(parsed_data, "Rides left: %u\n", rides_left);
        }
        furi_string_cat_printf(
            parsed_data, "TVM: %02X%02X ", data->page[7].data[1], data->page[7].data[0]);
        furi_string_cat_printf(parsed_data, "TxCnt: %u\n", context.highest_sequence);

        const bool first_is_current = context.current_block == 8;
        furi_string_cat_printf(
            parsed_data,
            "%s\n%s\n\n",
            furi_string_get_cstr(first_is_current ? transaction_1 : transaction_2),
            furi_string_get_cstr(first_is_current ? transaction_2 : transaction_1));
        parsed = true;
    } while(false);

    furi_string_free(transaction_2);
    furi_string_free(transaction_1);
    furi_string_free(product);
    return parsed;
}

static void ventra_show_data(Metroflip* app, const MfUltralightData* data, bool saved) {
    FuriString* parsed_data = furi_string_alloc();

    if(!ventra_parse(parsed_data, data)) {
        FURI_LOG_I(TAG, "Invalid or incomplete Ventra data");
        furi_string_printf(parsed_data, "\e#Unknown card\n");
    }

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
    widget_add_button_element(
        app->widget,
        saved ? GuiButtonTypeRight : GuiButtonTypeLeft,
        "Exit",
        metroflip_exit_widget_callback,
        app);
    widget_add_button_element(
        app->widget,
        saved ? GuiButtonTypeCenter : GuiButtonTypeRight,
        saved ? "Delete" : "Save",
        saved ? metroflip_delete_widget_callback : metroflip_save_widget_callback,
        app);

    furi_string_free(parsed_data);
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    metroflip_app_blink_stop(app);
}

static void ventra_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* format = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(format, app->file_path)) {
            MfUltralightData* data = mf_ultralight_alloc();
            if(mf_ultralight_load(data, format, 2)) {
                ventra_show_data(app, data, true);
            }
            mf_ultralight_free(data);
        }
        flipper_format_free(format);
        furi_record_close(RECORD_STORAGE);
    } else if(app->ultralight_data_ready) {
        const MfUltralightData* data =
            nfc_device_get_data(app->nfc_device, NfcProtocolMfUltralight);
        ventra_show_data(app, data, false);
    } else {
        FURI_LOG_E(TAG, "Ventra plugin entered without Ultralight data");
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventWrongCard);
    }
}

static bool ventra_on_event(Metroflip* app, SceneManagerEvent event) {
    UNUSED(app);
    UNUSED(event);
    return false;
}

static void ventra_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    popup_reset(app->popup);
    metroflip_app_blink_stop(app);
}

static const MetroflipPlugin ventra_plugin = {
    .card_name = "Ventra",
    .plugin_on_enter = ventra_on_enter,
    .plugin_on_event = ventra_on_event,
    .plugin_on_exit = ventra_on_exit,
};

static const FlipperAppPluginDescriptor ventra_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &ventra_plugin,
};

const FlipperAppPluginDescriptor* ventra_plugin_ep(void) {
    return &ventra_plugin_descriptor;
}
