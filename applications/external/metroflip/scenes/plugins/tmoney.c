/*
 * Parser for T-Money
 *
 * Copyright 2025 Justus Perlwitz <hello@justus.pw>
 *
 * This parses the balance on a T-Money card.
 *
 * The IOS7816Protcol class in Metrodroid served as a reference for this
 * implementation
 *
 * This plugin does not have a save feature
 *
 * References: https://github.com/metrodroid/metrodroid/blob/master/src/commonMain/kotlin/au/id/micolous/metrodroid/card/iso7816/ISO7816Protocol.kt
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

#include <dolphin/dolphin.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <lib/bit_lib/bit_lib.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include "../../metroflip_i.h"
#include "../../metroflip_plugins.h"
#include "../../api/metroflip/metroflip_api.h"

#define TAG "Metroflip:Scene:Tmoney"

// ISO7816 constants, check ISO7816Protocol.kt in metrodroid
#define CLASS_ISO7816              0x00
#define CLASS_90                   0x90
#define INSTRUCTION_ISO7816_SELECT 0xA4
#define SELECT_BY_NAME             0x04
#define STATUS_OK                  0x90
#define STATUS_OK_2                0x00

// From KSX6924Application.kt
#define INS_GET_BALANCE  0x4c
#define BALANCE_RESP_LEN 4

static int tmoney_send_iso7816_command(
    const uint8_t* command,
    size_t command_len,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    Iso14443_4aPoller* poller,
    Metroflip* app,
    MetroflipPollerEventType* stage) {
    if(!app->data_loaded) {
        FURI_LOG_I(TAG, "No data loaded");
        bit_buffer_reset(tx_buffer);
        bit_buffer_append_bytes(tx_buffer, command, command_len);
        Iso14443_4aError error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);
        if(error != Iso14443_4aErrorNone) {
            FURI_LOG_E(TAG, "iso14443_4a_poller_send_block error %d", error);
            *stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            return error;
        }
    }
    return 0;
}

static int check_response(
    BitBuffer* rx_buffer,
    Metroflip* app,
    MetroflipPollerEventType* stage,
    size_t* response_length) {
    *response_length = bit_buffer_get_size_bytes(rx_buffer);
    if(!app->data_loaded) { // automatic success
        if(bit_buffer_get_byte(rx_buffer, *response_length - 2) != STATUS_OK ||
           bit_buffer_get_byte(rx_buffer, *response_length - 1) != STATUS_OK_2) {
            int error_code_1 = bit_buffer_get_byte(rx_buffer, *response_length - 2);
            int error_code_2 = bit_buffer_get_byte(rx_buffer, *response_length - 1);
            FURI_LOG_E(TAG, "ISO7816 status: SW1=0x%02X SW2=0x%02X", error_code_1, error_code_2);
            // TODO implement better error checking here, see ISO7816Protocol.kt
            // in metrodroid
            *stage = MetroflipPollerEventTypeFail;
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFileNotFound);
            return 1;
        }
    }
    return 0;
}

static void delay(int milliseconds) {
    furi_thread_flags_wait(0, FuriFlagWaitAny, milliseconds);
}

static NfcCommand tmoney_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);
    NfcCommand next_command = NfcCommandContinue;
    MetroflipPollerEventType stage = MetroflipPollerEventTypeStart;

    Metroflip* app = context;
    FuriString* parsed_data = furi_string_alloc();
    Widget* widget = app->widget;
    furi_string_reset(app->text_box_store);

    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;

    Iso14443_4aPoller* iso14443_4a_poller = event.instance;

    BitBuffer* tx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);
    BitBuffer* rx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady || app->data_loaded) {
        if(stage == MetroflipPollerEventTypeStart) {
            // Start Flipper vibration
            NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notification, &sequence_set_vibro_on);
            delay(50);
            notification_message(notification, &sequence_reset_vibro);
            nfc_device_set_data(
                app->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(app->poller));

            Iso14443_4aError error;
            size_t response_length = 0;

            do {
                // Select T-Money app
                uint8_t select_cmd[] = {
                    CLASS_ISO7816,
                    INSTRUCTION_ISO7816_SELECT,
                    // select by name
                    SELECT_BY_NAME,
                    // first or only
                    0x00,
                    // Data is 7 bytes
                    0x07,
                    // Data=d4100000030001 (T-Money application name)
                    0xd4,
                    0x10,
                    0x00,
                    0x00,
                    0x03,
                    0x00,
                    0x01};

                error = tmoney_send_iso7816_command(
                    select_cmd,
                    sizeof(select_cmd),
                    tx_buffer,
                    rx_buffer,
                    iso14443_4a_poller,
                    app,
                    &stage);
                if(error != 0) {
                    FURI_LOG_E(TAG, "Failed to select T-Money application");
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, MetroflipCustomEventPollerFail);
                    next_command = NfcCommandStop;
                    break;
                }

                // Check the response after selecting app
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    FURI_LOG_W(TAG, "Failed to select T-Money application - not a T-Money card");
                    break;
                }

                // Send balance command: CLASS_90, get balance, P1=0, P2=0, Le=4
                uint8_t balance_cmd[] = {CLASS_90, INS_GET_BALANCE, 0x00, 0x00, 0x04};

                // Send read balance command
                error = tmoney_send_iso7816_command(
                    balance_cmd,
                    sizeof(balance_cmd),
                    tx_buffer,
                    rx_buffer,
                    iso14443_4a_poller,
                    app,
                    &stage);
                if(error != 0) {
                    FURI_LOG_W(TAG, "Failed to read balance");
                    break;
                }
                // Check the response after reading the balance
                if(check_response(rx_buffer, app, &stage, &response_length) != 0) {
                    break;
                }
                // + 2 for the status bytes
                if(response_length != BALANCE_RESP_LEN + 2) {
                    FURI_LOG_E(TAG, "Invalid balance reponse length %d", response_length);
                    break;
                }
                uint8_t balance_data[4] = {0};
                for(size_t i = 0; i < sizeof(balance_data); i++) {
                    balance_data[i] = bit_buffer_get_byte(rx_buffer, i);
                }

                FURI_LOG_I(TAG, "Balance read successfully");

                uint32_t balance_value = (balance_data[0] << 24) | (balance_data[1] << 16) |
                                         (balance_data[2] << 8) | balance_data[3];

                furi_string_printf(parsed_data, "Balance: %lu KRW", balance_value);

                widget_add_text_scroll_element(
                    widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));
                widget_add_button_element(
                    widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

                furi_string_free(parsed_data);
                view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
                metroflip_app_blink_stop(app);
                stage = MetroflipPollerEventTypeSuccess;
                next_command = NfcCommandStop;
            } while(false);

            if(stage != MetroflipPollerEventTypeSuccess) {
                next_command = NfcCommandStop;
            }
        }
    }

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    return next_command;
}

static void tmoney_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = app->popup;
    popup_set_header(popup, "Parsing...", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    nfc_scanner_alloc(app->nfc);
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(app->poller, tmoney_poller_callback, app);

    if(!app->data_loaded) {
        popup_reset(app->popup);
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        metroflip_app_blink_start(app);
    }
}

static bool tmoney_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipPollerEventTypeCardDetect) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Scanning..", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFileNotFound) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Read Error,\n wrong card", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail && app->data_loaded) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Bad File.", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFail) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Error, try\n again", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
        consumed = true;
    }

    return consumed;
}

static void tmoney_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    metroflip_app_blink_stop(app);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
}

static const MetroflipPlugin tmoney_plugin = {
    .card_name = "T-Money",
    .plugin_on_enter = tmoney_on_enter,
    .plugin_on_event = tmoney_on_event,
    .plugin_on_exit = tmoney_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor tmoney_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &tmoney_plugin,
};

const FlipperAppPluginDescriptor* tmoney_plugin_ep(void) {
    return &tmoney_plugin_descriptor;
}
