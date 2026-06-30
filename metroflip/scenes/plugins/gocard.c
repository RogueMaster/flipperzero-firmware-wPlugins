
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define TAG "Metroflip:Scene:gocard"

typedef enum {
    CHILD = 2051, // 0x803
    ADULT = 3073 // 0xc01
} ConcessionType;

bool hasTravelPassAvailable = false;

// Function to print concession type
void printConcessionType(unsigned short concession_type, FuriString* parsed_data) {
    switch(concession_type) {
    case CHILD:
        furi_string_cat_printf(parsed_data, "Concession Type: Child\n");
        break;
    case ADULT:
        furi_string_cat_printf(parsed_data, "Concession Type: Adult\n");
        break;
    default:
        furi_string_cat_printf(parsed_data, "Concession Type: 0x%X\n", concession_type);
        break;
    }
}

unsigned short byteArrayToIntReversed(unsigned int dec1, unsigned int dec2) {
    unsigned char byte1 = (unsigned char)dec1;
    unsigned char byte2 = (unsigned char)dec2;
    return ((unsigned short)byte2 << 8) | byte1;
}

// Function to extract a substring and convert binary to decimal
uint32_t extract_and_convert(const char* str, int start, int length) {
    uint32_t value = 0;
    for(int i = 0; i < length; i++) {
        if(str[start + i] == '1') {
            value |= (1U << (length - 1 - i));
        }
    }
    return value;
}

void parse_gocard_time(int block, int offset, const MfClassicData* data, FuriString* parsed_data) {
    //byte to start at
    int num_bytes = 4;
    char gocard_date_bit_representation[num_bytes * 8 + 1];
    memset(gocard_date_bit_representation, 0, sizeof(gocard_date_bit_representation));

    for(int i = (offset + num_bytes - 1), j = 0; i >= offset;
        i--, j++) { // Reverse the order of bytes and converty to binary
        char bits[9];
        byte_to_binary(data->block[block].data[i], bits);
        memcpy(&gocard_date_bit_representation[j * 8], bits, 8);
    }
    gocard_date_bit_representation[num_bytes * 8] = '\0';

    int len = strlen(gocard_date_bit_representation);
    FURI_LOG_I(TAG, "len %d", len); // I get 34

    if(len != 32 && len != 33) {
        FURI_LOG_I(TAG, "Invalid input length");
        return;
    }

    // Field layout (from rightmost bit):
    // - Day: 5 bits
    // - Month: 4 bits
    // - Year: 6 bits (years since 2000)
    // - Minutes: 11 bits (minutes from midnight)
    // Extract values from right to left using bit_slice_to_dec
    uint32_t day = bit_slice_to_dec(gocard_date_bit_representation, len - 5, len);
    uint32_t month = bit_slice_to_dec(gocard_date_bit_representation, len - 9, len - 6);
    uint32_t year = bit_slice_to_dec(gocard_date_bit_representation, len - 15, len - 10);
    uint32_t minutes = bit_slice_to_dec(gocard_date_bit_representation, len - 26, len - 16);

    // Convert year from offset 2000
    year += 2000;

    // Convert minutes since midnight to HH:MM
    uint32_t hours = minutes / 60;
    uint32_t mins = minutes % 60;

    // Format output string: "YYYY-MM-DD HH:MM"
    furi_string_cat_printf(
        parsed_data, "%04lu-%02lu-%02lu %02lu:%02lu\n", year, month, day, hours, mins);
}

void parse_gocard_topup_info(FuriString* parsed_data, const MfClassicData* data) {
    furi_string_cat_printf(parsed_data, "\n\e#Top-Up Info:");
    bool fully_empty = true;
    int block_num = 8;
    for(int i = block_num; i < block_num + 3; i++) {
        /******* Check if it's empty ******/
        bool is_block_empty = true;

        for(int j = 2; j < 8; j++) {
            if(data->block[i].data[j] != 0) {
                FURI_LOG_I(TAG, "Not 0, proceeding");
                is_block_empty = false;
                break;
            }
        }

        if(is_block_empty) {
            FURI_LOG_I(TAG, "Block %d is empty", i);
            continue;
        } else {
            fully_empty = false;
        }

        /**** If not fully empty, proceed ******/
        unsigned short creditcents =
            byteArrayToIntReversed(data->block[i].data[6], data->block[i].data[7]);
        creditcents &= 0x7FFF;
        double credit = creditcents / 100.0;

        furi_string_cat_printf(parsed_data, "\nCredit Added: A$%.2f\nTime: ", credit);
        parse_gocard_time(i, 2, data, parsed_data);
    }

    if(fully_empty) {
        FURI_LOG_I(TAG, "All checked blocks are empty, returning.");
    }
}

static bool gocard_parse(FuriString* parsed_data, const MfClassicData* data) {
    bool parsed = false;

    do {
        int balance_slot = 4;

        if(data->block[balance_slot].data[13] <= data->block[balance_slot + 1].data[13])
            balance_slot++;

        unsigned short balancecents = byteArrayToIntReversed(
            data->block[balance_slot].data[2], data->block[balance_slot].data[3]);

        // Check if the sign flag is set in 'balance'
        if((balancecents & 0x8000) == 0x8000) {
            balancecents = balancecents & 0x7fff; // Clear the sign flag.
            balancecents *= -1; // Negate the balance.
        }
        // Otherwise, check the sign flag in data->block[4].data[1]
        else if((data->block[balance_slot].data[1] & 0x80) == 0x80) {
            // seq_go uses a sign flag in an adjacent byte.
            balancecents *= -1;
        }

        double balance = balancecents / 100.0;
        furi_string_printf(parsed_data, "\e#go card\nValue: A$%.2f\n", balance); //show balance

        hasTravelPassAvailable = (data->block[balance_slot].data[7] != 0x00) ? true : false;
        int start_index = 4; //byte to start at
        int config_block = 6; //block number containing card configuration

        furi_string_cat_printf(parsed_data, "Expiry:\n");
        parse_gocard_time(config_block, start_index, data, parsed_data);

        //concession type:

        unsigned short concession_type = byteArrayToIntReversed(
            data->block[config_block].data[8], data->block[config_block].data[9]);

        printConcessionType(concession_type, parsed_data);

        parse_gocard_topup_info(parsed_data, data);

        parsed = true;
    } while(false);

    return parsed;
}

static void gocard_on_enter(Metroflip* app) {
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
            if(!gocard_parse(parsed_data, mfc_data)) {
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
        popup_set_header(popup, "unsupported", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
    }
}

static bool gocard_on_event(Metroflip* app, SceneManagerEvent event) {
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

static void gocard_on_exit(Metroflip* app) {
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
static const MetroflipPlugin gocard_plugin = {
    .card_name = "gocard",
    .plugin_on_enter = gocard_on_enter,
    .plugin_on_event = gocard_on_event,
    .plugin_on_exit = gocard_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor gocard_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &gocard_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* gocard_plugin_ep(void) {
    return &gocard_plugin_descriptor;
}
