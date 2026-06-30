
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

#define TAG "Metroflip:Scene:TwoCities"

static const MfClassicKeyPair two_cities_4k_keys[] = {
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99}, {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.a = 0xe56ac127dd45, .b = 0x19fc84a3784b}, {.a = 0x77dabc9825e1, .b = 0x9764fec3154a},
    {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99}, {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xa73f5dc1d333, .b = 0xe35173494a81}, {.a = 0x69a32f1c2f19, .b = 0x6b8bd9860763},
    {.a = 0xea0fd73cb149, .b = 0x29c35fa068fb}, {.a = 0xc76bf71a2509, .b = 0x9ba241db3f56},
    {.a = 0xacffffffffff, .b = 0x71f3a315ad26}, {.a = 0xffffffffffff, .b = 0xffffffffffff},
    {.a = 0xffffffffffff, .b = 0xffffffffffff}, {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.a = 0x72f96bdd3714, .b = 0x462225cd34cf}, {.a = 0x044ce1872bc3, .b = 0x8c90c70cff4a},
    {.a = 0xbc2d1791dec1, .b = 0xca96a487de0b}, {.a = 0x8791b2ccb5c4, .b = 0xc956c3b80da3},
    {.a = 0x8e26e45e7d65, .b = 0x8e65b3af7d22}, {.a = 0x0f318130ed18, .b = 0x0c420a20e056},
    {.a = 0x045ceca15535, .b = 0x31bec3d9e510}, {.a = 0x9d993c5d4ef4, .b = 0x86120e488abf},
    {.a = 0xc65d4eaa645b, .b = 0xb69d40d1a439}, {.a = 0x3a8a139c20b4, .b = 0x8818a9c5d406},
    {.a = 0xbaff3053b496, .b = 0x4b7cb25354d3}, {.a = 0x7413b599c4ea, .b = 0xb0a2AAF3A1BA},
    {.a = 0x0ce7cd2cc72b, .b = 0xfa1fbb3f0f1f}, {.a = 0x0be5fac8b06a, .b = 0x6f95887a4fd3},
    {.a = 0x26973ea74321, .b = 0xd27058c6e2c7}, {.a = 0xeb0a8ff88ade, .b = 0x578a9ada41e3},
    {.a = 0x7a396f0d633d, .b = 0xad2bdc097023}, {.a = 0xa3faa6daff67, .b = 0x7600e889adf9},
    {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99}, {.a = 0x2aa05ed1856f, .b = 0xeaac88e5dc99},
    {.a = 0xa7141147d430, .b = 0xff16014fefc7}, {.a = 0x8a8d88151a00, .b = 0x038b5f9b5a2a},
    {.a = 0xb27addfb64b0, .b = 0x152fd0c420a7}, {.a = 0x7259fa0197c6, .b = 0x5583698df085},
};

static bool two_cities_parse(FuriString* parsed_data, const MfClassicData* data) {
    bool parsed = false;

    do {
        // Verify key
        MfClassicSectorTrailer* sec_tr = mf_classic_get_sector_trailer_by_sector(data, 4);
        uint64_t key = bit_lib_bytes_to_num_be(sec_tr->key_a.data, 6);
        if(key != two_cities_4k_keys[4].a) return false;

        // =====
        // PLANTAIN
        // =====

        // Point to block 0 of sector 4, value 0
        const uint8_t* temp_ptr = data->block[16].data;
        // Read first 4 bytes of block 0 of sector 4 from last to first and convert them to uint32_t
        // 38 18 00 00 becomes 00 00 18 38, and equals to 6200 decimal
        uint32_t balance =
            ((temp_ptr[3] << 24) | (temp_ptr[2] << 16) | (temp_ptr[1] << 8) | temp_ptr[0]) / 100;
        // Read card number
        // Point to block 0 of sector 0, value 0
        temp_ptr = data->block[0].data;
        // Read first 7 bytes of block 0 of sector 0 from last to first and convert them to uint64_t
        // 04 31 16 8A 23 5C 80 becomes 80 5C 23 8A 16 31 04, and equals to 36130104729284868 decimal
        uint8_t card_number_arr[7];
        for(size_t i = 0; i < 7; i++) {
            card_number_arr[i] = temp_ptr[6 - i];
        }
        // Copy card number to uint64_t
        uint64_t card_number = 0;
        for(size_t i = 0; i < 7; i++) {
            card_number = (card_number << 8) | card_number_arr[i];
        }

        // =====
        // --PLANTAIN--
        // =====
        // TROIKA
        // =====

        const uint8_t* troika_temp_ptr = &data->block[33].data[5];
        uint16_t troika_balance = ((troika_temp_ptr[0] << 8) | troika_temp_ptr[1]) / 25;
        troika_temp_ptr = &data->block[32].data[2];
        uint32_t troika_number = 0;
        for(size_t i = 0; i < 4; i++) {
            troika_number <<= 8;
            troika_number |= troika_temp_ptr[i];
        }
        troika_number >>= 4;

        furi_string_printf(
            parsed_data,
            "\e#Troika+Plantain\nPN: %lluX\nPB: %lu rur.\nTN: %lu\nTB: %u rur.\n",
            card_number,
            balance,
            troika_number,
            troika_balance);

        parsed = true;
    } while(false);

    return parsed;
}

static NfcCommand two_cities_poller_callback(NfcGenericEvent event, void* context) {
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
        bit_lib_num_to_bytes_be(two_cities_4k_keys[app->sec_num].a, COUNT_OF(key.data), key.data);

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
        two_cities_parse(parsed_data, mfc_data);
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);
        widget_add_button_element(
            widget, GuiButtonTypeCenter, "Save", metroflip_save_widget_callback, app);

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        command = NfcCommandStop;
        metroflip_app_blink_stop(app);
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

static void two_cities_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);
    FURI_LOG_I(TAG, "open TwoCities");
    app->sec_num = 0;

    if(app->data_loaded) {
        FURI_LOG_I(TAG, "TwoCities loaded");
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfClassicData* mfc_data = mf_classic_alloc();
            mf_classic_load(mfc_data, ff, 2);
            FuriString* parsed_data = furi_string_alloc();
            Widget* widget = app->widget;

            furi_string_reset(app->text_box_store);
            if(!two_cities_parse(parsed_data, mfc_data)) {
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
        FURI_LOG_I(TAG, "TwoCities not loaded");
        // Setup view
        Popup* popup = app->popup;
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

        // Start worker
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
        nfc_poller_start(app->poller, two_cities_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool two_cities_on_event(Metroflip* app, SceneManagerEvent event) {
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

static void two_cities_on_exit(Metroflip* app) {
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
static const MetroflipPlugin two_cities_plugin = {
    .card_name = "TwoCities",
    .plugin_on_enter = two_cities_on_enter,
    .plugin_on_event = two_cities_on_event,
    .plugin_on_exit = two_cities_on_exit,

};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor two_cities_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &two_cities_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* two_cities_plugin_ep(void) {
    return &two_cities_plugin_descriptor;
}
