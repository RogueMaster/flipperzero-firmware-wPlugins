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

/* Balance read by the poller thread, displayed by the main thread on
   MetroflipCustomEventPollerSuccess. Plugin .fal data, reset on every load. */
static uint32_t tmoney_balance_krw = 0;

static int tmoney_send_iso7816_command(
    const uint8_t* command,
    size_t command_len,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer,
    Iso14443_4aPoller* poller,
    Metroflip* app,
    MetroflipPollerEventType* stage) {
    bit_buffer_reset(tx_buffer);
    bit_buffer_append_bytes(tx_buffer, command, command_len);
    Iso14443_4aError error = iso14443_4a_poller_send_block(poller, tx_buffer, rx_buffer);
    if(error != Iso14443_4aErrorNone) {
        FURI_LOG_E(TAG, "iso14443_4a_poller_send_block error %d", error);
        *stage = MetroflipPollerEventTypeFail;
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
        return error;
    }
    return 0;
}

static int check_response(
    BitBuffer* rx_buffer,
    Metroflip* app,
    MetroflipPollerEventType* stage,
    size_t* response_length) {
    *response_length = bit_buffer_get_size_bytes(rx_buffer);
    /* A response shorter than the two status bytes would underflow the
       indices below (size_t wraps) and crash in bit_buffer_get_byte. */
    if(*response_length < 2) {
        FURI_LOG_E(TAG, "ISO7816 response too short: %zu bytes", *response_length);
        *stage = MetroflipPollerEventTypeFail;
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerFail);
        return 1;
    }
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

    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;
    Iso14443_4aPoller* iso14443_4a_poller = event.instance;

    if(iso14443_4a_event->type != Iso14443_4aPollerEventTypeReady) {
        return NfcCommandContinue;
    }

    BitBuffer* tx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);
    BitBuffer* rx_buffer = bit_buffer_alloc(Metroflip_POLLER_MAX_BUFFER_SIZE);

    // Start Flipper vibration
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notification, &sequence_set_vibro_on);
    delay(50);
    notification_message(notification, &sequence_reset_vibro);
    furi_record_close(RECORD_NOTIFICATION);

    nfc_device_set_data(app->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(app->poller));

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
            select_cmd, sizeof(select_cmd), tx_buffer, rx_buffer, iso14443_4a_poller, app, &stage);
        if(error != 0) {
            FURI_LOG_E(TAG, "Failed to select T-Money application");
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
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MetroflipCustomEventPollerFail);
            break;
        }
        uint8_t balance_data[4] = {0};
        for(size_t i = 0; i < sizeof(balance_data); i++) {
            balance_data[i] = bit_buffer_get_byte(rx_buffer, i);
        }

        FURI_LOG_I(TAG, "Balance read successfully");

        tmoney_balance_krw = (balance_data[0] << 24) | (balance_data[1] << 16) |
                             (balance_data[2] << 8) | balance_data[3];

        /* Display happens on the main thread (PollerSuccess handler) - never
           build UI or switch views from the NFC worker thread. */
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipCustomEventPollerSuccess);
        stage = MetroflipPollerEventTypeSuccess;
    } while(false);

    next_command = NfcCommandStop;

    bit_buffer_free(tx_buffer);
    bit_buffer_free(rx_buffer);

    return next_command;
}

static void tmoney_show_card_view(Metroflip* app) {
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "T-Money");

    uint8_t p = metroflip_card_view_add_page(view, "Card Info");

    char val[METROFLIP_CARD_VIEW_VALUE_LEN];
    snprintf(val, sizeof(val), "%lu KRW", (unsigned long)tmoney_balance_krw);
    metroflip_card_view_add_field(view, p, "Balance", val, true);

    /* No save: only the balance is read over ISO7816, there is no
       re-parseable card image to store. */
    metroflip_card_view_show(app);
}

static void tmoney_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        /* T-Money is read over live ISO7816 commands only - there is no
           saved-file parse path (and the load scene never routes here). */
        View* view = metroflip_card_view_alloc(app);
        metroflip_card_view_set_title(view, "T-Money");
        uint8_t p = metroflip_card_view_add_page(view, "");
        metroflip_card_view_add_field(view, p, "Status", "Saved files not supported", false);
        metroflip_card_view_show(app);
        return;
    }

    // Setup view
    Popup* popup = app->popup;
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);
    popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);

    // Start worker
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(app->poller, tmoney_poller_callback, app);

    metroflip_app_blink_start(app);
}

static bool tmoney_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Release the NFC hardware before showing results (the worker
               returned NfcCommandStop; stop+free is still our job). */
            if(app->poller) {
                nfc_poller_stop(app->poller);
                nfc_poller_free(app->poller);
                app->poller = NULL;
            }
            metroflip_app_blink_stop(app);
            tmoney_show_card_view(app);
            consumed = true;
        } else if(event.event == MetroflipPollerEventTypeCardDetect) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Scanning..", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MetroflipCustomEventPollerFileNotFound) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Read Error,\n wrong card", 68, 30, AlignLeft, AlignTop);
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
    popup_reset(app->popup);
    metroflip_app_blink_stop(app);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
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
