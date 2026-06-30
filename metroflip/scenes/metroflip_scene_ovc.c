
#include <flipper_application.h>
#include "../metroflip_i.h"

#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include "../api/metroflip/metroflip_api.h"

#include <dolphin/dolphin.h>
#include <bit_lib.h>
#include <furi_hal.h>
#include <nfc/nfc.h>
#include <nfc/nfc_device.h>
#include <nfc/nfc_listener.h>

typedef struct {
    int year;
    int month;
    int day;
} OvcBcdDate;

int bcd2int(int bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

#define TAG "Metroflip:Scene:OVC"

uint8_t ovc_sector_num = 0;

OvcBcdDate* ovc_bcd_date_new(int x) {
    OvcBcdDate* date = malloc(sizeof(OvcBcdDate));
    if(!date) {
        FURI_LOG_I(TAG, "Failed to allocate memory");
        return NULL;
    }

    date->day = bcd2int((x >> 0) & 0xFF);
    date->month = bcd2int((x >> 8) & 0xFF);
    date->year = bcd2int((x >> 16) & 0xFFFF);

    // Check if the year is valid
    if(date->year == 0) {
        free(date);
        return NULL;
    }

    return date;
}

static bool ovc_parse(FuriString* parsed_data, const MfClassicData* data) {
    bool parsed = false;

    do {
        // Parse data
        //Card ID (UID in Dec)
        size_t uid_len = 0;
        const uint8_t* uid = mf_classic_get_uid(data, &uid_len);
        uint32_t cardID = bit_lib_bytes_to_num_le(uid, 4);
        const MfClassicBlock* block1 = &data->block[1]; // basic info block
        const MfClassicBlock* block2 = &data->block[2]; // basic info block
        char bit_representation[32 * 8 + 1];
        bit_representation[0] = '\0';
        for(size_t i = 0; i < 32; i++) {
            char bits[9];
            uint8_t byte = 0;

            if(i < 16) { // First block
                byte = block1->data[i];
            } else { // Second block
                byte = block2->data[i - 16];
            }

            byte_to_binary(byte, bits);
            strlcat(bit_representation, bits, sizeof(bit_representation));
        }
        bit_representation[32 * 8] = '\0';
        FURI_LOG_I(TAG, "bit_repr %s", bit_representation);
        int card_type = bit_slice_to_dec(bit_representation, 145, 151);
        int days_from_epoch = bit_slice_to_dec(bit_representation, 95, 107);
        FURI_LOG_I(TAG, "%d", days_from_epoch);
        int seconds_with_epoch = (days_from_epoch * 3600 * 24) + epoch + 3600;

        furi_string_printf(
            parsed_data, "\e#Ov-Chipkaart\nCard ID: %lu\nCard Type: %d", cardID, card_type);
        DateTime dt = {0};
        datetime_timestamp_to_datetime(seconds_with_epoch, &dt);
        furi_string_cat_printf(parsed_data, "\nEnd Validity:\n");
        locale_format_datetime_cat(parsed_data, &dt, true);
        furi_string_cat_printf(parsed_data, "\n\n");

        const MfClassicBlock* block248 = &data->block[248];
        memset(bit_representation, 0, sizeof(bit_representation));
        for(size_t i = 0; i < 16; i++) {
            char bits[9];
            uint8_t byte = 0;
            byte = block248->data[i];
            byte_to_binary(byte, bits);
            strlcat(bit_representation, bits, sizeof(bit_representation));
        }
        int first_3_bits = bit_slice_to_dec(bit_representation, 0, 3);
        int credit_slot = (first_3_bits & 1) ? 250 : 249; // False is block 249 True is block 250
        FURI_LOG_I(TAG, "credit block: %d", credit_slot);
        const MfClassicBlock* credit_block = &data->block[credit_slot];
        memset(bit_representation, 0, sizeof(bit_representation));
        for(size_t i = 0; i < 16; i++) {
            char bits[9];
            uint8_t byte = 0;
            byte = credit_block->data[i];
            byte_to_binary(byte, bits);
            strlcat(bit_representation, bits, sizeof(bit_representation));
        }
        // credit
        float credit = bit_slice_to_dec(bit_representation, 78, 92) / 100.0;
        FURI_LOG_I(TAG, "bit_repr block credit: %s", bit_representation);
        FURI_LOG_I(TAG, "credit: %f", (double)credit);
        furi_string_cat_printf(parsed_data, "Credit: %f", (double)credit);
        // birthdate
        OvcBcdDate* date = ovc_bcd_date_new(bit_slice_to_dec(bit_representation, 78, 92));
        if(card_type == 2) {
            furi_string_cat_printf(
                parsed_data, "Date: %04d-%02d-%02d\n", date->year, date->month, date->day);
        }
        for(int sector = 32; sector < 35; sector++) {
            memset(bit_representation, 0, sizeof(bit_representation));
            char bit_representation[240 * 8 + 1]; // 15 x 16 bytes (the whole sector - sector trailer)
            bit_representation[0] = '\0';
            for(int block = (16 * (sector - 32) + 128); block < (16 * (sector - 32) + 143);
                block++) {
                const MfClassicBlock* current_block = &data->block[block];
                for(size_t i = 0; i < 16; i++) {
                    char bits[9];
                    uint8_t byte = 0;
                    byte = current_block->data[i];

                    byte_to_binary(byte, bits);
                    strlcat(bit_representation, bits, sizeof(bit_representation));
                }
            }
            bit_representation[240 * 8] = '\0';
            FURI_LOG_I(TAG, "sector %d bit repr: %s\n", sector, bit_representation);
        }
        parsed = true;
    } while(false);

    return parsed;
}

static NfcCommand metroflip_scene_ovc_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.event_data);
    furi_assert(event.protocol == NfcProtocolMfClassic);

    NfcCommand command = NfcCommandContinue;
    const MfClassicPollerEvent* mfc_event = event.event_data;
    Metroflip* app = context;
    FuriString* parsed_data = furi_string_alloc();
    Widget* widget = app->widget;

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
        nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        size_t uid_len = 0;
        const uint8_t* uid = nfc_device_get_uid(app->nfc_device, &uid_len);
        /*-----------------All of this is to store a keyfile in a permanent way for the user to always access------------*/
        /*-----------------Open cache file (if exists)------------*/

        char uid_str[uid_len * 2 + 1];
        uid_to_string(uid, uid_len, uid_str, sizeof(uid_str));
        uint64_t ovc_key_mask_a_required =
            1095220854785; // 1111111100000000010000000000000000000001 key mask
        KeyfileManager manage =
            manage_keyfiles(uid_str, uid, uid_len, app->mfc_key_cache, ovc_key_mask_a_required, 0);
        char card_type[] = "OV-Chipkaart";
        switch(manage) {
        case MISSING_KEYFILE:
            handle_keyfile_case(app, "No keys found", "Missing keyfile", parsed_data, card_type);
            command = NfcCommandStop;
            break;

        case INCOMPLETE_KEYFILE:
            handle_keyfile_case(
                app, "Incomplete keyfile", "incomplete keyfile", parsed_data, card_type);
            command = NfcCommandStop;
            break;

        case SUCCESSFUL:
            FURI_LOG_I(TAG, "success");
            break;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestReadSector) {
        FURI_LOG_I(TAG, "sec_num: %d", ovc_sector_num);
        MfClassicKey key = {};
        MfClassicKeyType key_type = MfClassicKeyTypeA;
        if(mf_classic_key_cache_get_next_key(
               app->mfc_key_cache, &ovc_sector_num, &key, &key_type)) {
            mfc_event->data->read_sector_request_data.sector_num = ovc_sector_num;
            mfc_event->data->read_sector_request_data.key = key;
            mfc_event->data->read_sector_request_data.key_type = key_type;
            mfc_event->data->read_sector_request_data.key_provided = true;
        } else {
            mfc_event->data->read_sector_request_data.key_provided = false;
        }
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));

        dolphin_deed(DolphinDeedNfcReadSuccess);
        furi_string_reset(app->text_box_store);
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        if(!ovc_parse(parsed_data, mfc_data)) {
            FURI_LOG_I(TAG, "Unknown card type");
            furi_string_printf(parsed_data, "\e#Unknown card\n");
        }
        widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

        widget_add_button_element(
            widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

        furi_string_free(parsed_data);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
        metroflip_app_blink_stop(app);
        UNUSED(ovc_parse);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_I(TAG, "fail");
        command = NfcCommandStop;
    }

    return command;
}

void metroflip_scene_ovc_on_enter(void* context) {
    Metroflip* app = context;
    dolphin_deed(DolphinDeedNfcRead);

    mf_classic_key_cache_reset(app->mfc_key_cache);

    // Setup view
    Popup* popup = app->popup;
    popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
    nfc_scanner_alloc(app->nfc);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
    nfc_poller_start(app->poller, metroflip_scene_ovc_poller_callback, app);

    metroflip_app_blink_start(app);
}

bool metroflip_scene_ovc_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
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

void metroflip_scene_ovc_on_exit(void* context) {
    Metroflip* app = context;
    widget_reset(app->widget);

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }

    // Clear view
    popup_reset(app->popup);

    metroflip_app_blink_stop(app);
}
