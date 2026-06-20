
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

/* Parse gocard date/time from a block and write formatted string into out buffer */
static void
    gocard_format_time(int block, int offset, const MfClassicData* data, char* out, size_t out_len) {
    int num_bytes = 4;
    char gocard_date_bit_representation[num_bytes * 8 + 1];
    memset(gocard_date_bit_representation, 0, sizeof(gocard_date_bit_representation));

    for(int i = (offset + num_bytes - 1), j = 0; i >= offset; i--, j++) {
        char bits[9];
        byte_to_binary(data->block[block].data[i], bits);
        memcpy(&gocard_date_bit_representation[j * 8], bits, 8);
    }
    gocard_date_bit_representation[num_bytes * 8] = '\0';

    int len = strlen(gocard_date_bit_representation);

    if(len != 32 && len != 33) {
        snprintf(out, out_len, "Invalid");
        return;
    }

    uint32_t day = bit_slice_to_dec(gocard_date_bit_representation, len - 5, len);
    uint32_t month = bit_slice_to_dec(gocard_date_bit_representation, len - 9, len - 6);
    uint32_t year = bit_slice_to_dec(gocard_date_bit_representation, len - 15, len - 10);
    uint32_t minutes = bit_slice_to_dec(gocard_date_bit_representation, len - 26, len - 16);

    year += 2000;

    uint32_t hours = minutes / 60;
    uint32_t mins = minutes % 60;

    snprintf(out, out_len, "%04lu-%02lu-%02lu %02lu:%02lu", year, month, day, hours, mins);
}

/* Get concession type string, NULL if unknown (caller shows raw value) */
static const char* gocard_concession_str(unsigned short concession_type) {
    switch(concession_type) {
    case CHILD:
        return "Child";
    case ADULT:
        return "Adult";
    default:
        return NULL;
    }
}

/* Parse and populate card view. Returns true on success. */
static bool gocard_display_card_view(const MfClassicData* data, Metroflip* app, bool from_file) {
    int balance_slot = 4;

    if(data->block[balance_slot].data[13] <= data->block[balance_slot + 1].data[13])
        balance_slot++;

    unsigned short balancecents = byteArrayToIntReversed(
        data->block[balance_slot].data[2], data->block[balance_slot].data[3]);

    // Check if the sign flag is set in 'balance'
    if((balancecents & 0x8000) == 0x8000) {
        balancecents = balancecents & 0x7fff;
        balancecents *= -1;
    } else if((data->block[balance_slot].data[1] & 0x80) == 0x80) {
        balancecents *= -1;
    }

    double balance = balancecents / 100.0;

    bool has_travel_pass = (data->block[balance_slot].data[7] != 0x00);
    int config_block = 6;

    unsigned short concession_type = byteArrayToIntReversed(
        data->block[config_block].data[8], data->block[config_block].data[9]);

    /* Allocate card view */
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "go card");

    /* Page: Overview */
    uint8_t p = metroflip_card_view_add_page(view, "Overview");
    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    snprintf(val, sizeof(val), "A$%.2f", balance);
    metroflip_card_view_add_field(view, p, "Balance", val, true);

    gocard_format_time(config_block, 4, data, val, sizeof(val));
    metroflip_card_view_add_field(view, p, "Expiry", val, false);

    const char* concession = gocard_concession_str(concession_type);
    if(!concession) {
        snprintf(val, sizeof(val), "0x%X", concession_type);
        concession = val;
    }
    metroflip_card_view_add_field(view, p, "Concession", concession, false);

    if(has_travel_pass) {
        metroflip_card_view_add_field(view, p, "Travel Pass", "Available", false);
    }

    /* Top-Up History pages */
    int block_num = 8;
    for(int i = block_num; i < block_num + 3; i++) {
        bool is_block_empty = true;
        for(int j = 2; j < 8; j++) {
            if(data->block[i].data[j] != 0) {
                is_block_empty = false;
                break;
            }
        }
        if(is_block_empty) continue;

        unsigned short creditcents =
            byteArrayToIntReversed(data->block[i].data[6], data->block[i].data[7]);
        creditcents &= 0x7FFF;
        double credit = creditcents / 100.0;

        char hdr[METROFLIP_CARD_VIEW_HEADER_LEN];
        snprintf(hdr, sizeof(hdr), "Top-Up %d", i - block_num + 1);
        p = metroflip_card_view_add_page(view, hdr);

        snprintf(val, sizeof(val), "A$%.2f", credit);
        metroflip_card_view_add_field(view, p, "Credit Added", val, true);

        gocard_format_time(i, 2, data, val, sizeof(val));
        metroflip_card_view_add_field(view, p, "Time", val, false);
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

static void gocard_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    app->sec_num = 0;

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);

            if(!gocard_display_card_view(mfc_data, app, true)) {
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
        // Live reading is not implemented for go card
        Widget* widget = app->widget;
        widget_add_text_scroll_element(
            widget,
            0,
            0,
            128,
            52,
            "\e#go card\n\nLive reading not\nsupported.\n\nSave the card with\nthe NFC app, then\nopen the file here.");
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    }
}

static bool gocard_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventCardDetected) {
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

static void gocard_on_exit(Metroflip* app) {
    widget_reset(app->widget);
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
