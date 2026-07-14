/*
 * opal.c - Parser for Opal card (Sydney, Australia).
 *
 * Copyright 2023 Michael Farrell <micolous+git@gmail.com>
 *
 * This will only read "standard" MIFARE DESFire-based Opal cards.
 *
 * Reference: https://github.com/metrodroid/metrodroid/wiki/Opal
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

#include "../../metroflip_i.h"
#include <flipper_application.h>

#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include <lib/nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include "../../api/metroflip/metroflip_api.h"
#include "../../metroflip_plugins.h"

#include <applications/services/locale/locale.h>
#include <datetime.h>

#define TAG "Metroflip:Scene:Opal"

static const MfDesfireApplicationId opal_app_id = {.data = {0x31, 0x45, 0x53}};

static const MfDesfireFileId opal_file_id = 0x07;

static const char* opal_modes[5] =
    {"Rail / Metro", "Ferry / Light Rail", "Bus", "Unknown mode", "Manly Ferry"};

// Strings kept <= 31 chars (METROFLIP_CARD_VIEW_VALUE_LEN - 1).
static const char* opal_usages[14] = {
    "New / Unused",
    "Tap on: new journey",
    "Tap on: transfer same mode",
    "Tap on: transfer other mode",
    NULL,
    NULL,
    NULL,
    "Tap off: distance fare",
    "Tap off: flat fare",
    "Auto tap off: failed to tap off",
    "Tap off: trip without start",
    "Tap off: reversal",
    "Tap on: rejected",
    "Unknown usage",
};

// Opal file 0x7 structure. Assumes a little-endian CPU.
typedef struct FURI_PACKED {
    uint32_t serial         : 32;
    uint8_t check_digit     : 4;
    bool blocked            : 1;
    uint16_t txn_number     : 16;
    int32_t balance         : 21;
    uint16_t days           : 15;
    uint16_t minutes        : 11;
    uint8_t mode            : 3;
    uint16_t usage          : 4;
    bool auto_topup         : 1;
    uint8_t weekly_journeys : 4;
    uint16_t checksum       : 16;
} OpalFile;

static_assert(sizeof(OpalFile) == 16, "OpalFile");

static void opal_days_minutes_to_datetime(uint16_t days, uint16_t minutes, DateTime* out) {
    out->year = 1980;
    out->month = 1;
    out->weekday = ((days + 1) % 7) + 1;
    out->hour = minutes / 60;
    out->minute = minutes % 60;
    out->second = 0;

    for(;;) {
        const uint16_t num_days_in_year = datetime_get_days_per_year(out->year);
        if(days < num_days_in_year) break;
        days -= num_days_in_year;
        out->year++;
    }
    days++;
    for(;;) {
        const bool is_leap = datetime_is_leap_year(out->year);
        const uint8_t num_days_in_month = datetime_get_days_per_month(is_leap, out->month);
        if(days <= num_days_in_month) break;
        days -= num_days_in_month;
        out->month++;
    }
    out->day = days;
}

static bool opal_display_card_view(const MfDesfireData* data, Metroflip* app, bool from_file) {
    const MfDesfireApplication* mf_app = mf_desfire_get_application(data, &opal_app_id);
    if(mf_app == NULL) return false;

    const MfDesfireFileSettings* file_settings =
        mf_desfire_get_file_settings(mf_app, &opal_file_id);
    if(file_settings == NULL || file_settings->type != MfDesfireFileTypeStandard ||
       file_settings->data.size != sizeof(OpalFile))
        return false;

    const MfDesfireFileData* file_data = mf_desfire_get_file_data(mf_app, &opal_file_id);
    if(file_data == NULL) return false;

    const OpalFile* opal = simple_array_cget_data(file_data->data);

    if(opal->check_digit > 9) return false;

    /* Card view setup */
    View* view = metroflip_card_view_alloc(app);
    metroflip_card_view_set_title(view, "Opal");

    char val[METROFLIP_CARD_VIEW_VALUE_LEN];

    /* ── Page: Card Info ── */
    uint8_t p = metroflip_card_view_add_page(view, "Card Info");

    const uint8_t s2 = opal->serial / 10000000;
    const uint16_t s3 = (opal->serial / 1000) % 10000;
    const uint16_t s4 = opal->serial % 1000;
    snprintf(val, sizeof(val), "3085 22%02u %04u %03u%u", s2, s3, s4, opal->check_digit);
    metroflip_card_view_add_field(view, p, "Serial", val, false);

    const bool neg = (opal->balance < 0);
    const int32_t abs_bal = neg ? labs(opal->balance) : opal->balance;
    snprintf(
        val,
        sizeof(val),
        "%s$%ld.%02u",
        neg ? "-" : "",
        (long)(abs_bal / 100),
        (unsigned)(abs_bal % 100));
    metroflip_card_view_add_field(view, p, "Balance", val, true);

    metroflip_card_view_add_field(view, p, "Status", opal->blocked ? "Blocked" : "Active", false);

    metroflip_card_view_add_field(
        view, p, "Auto Top-up", opal->auto_topup ? "Enabled" : "Disabled", false);

    /* ── Page: Last Activity ── */
    p = metroflip_card_view_add_page(view, "Last Activity");

    const bool is_manly = (opal->usage >= 4) && (opal->usage <= 6);
    const uint8_t mode = is_manly ? 4 : opal->mode;
    const uint8_t usage = is_manly ? opal->usage - 3 : opal->usage;

    metroflip_card_view_add_field(view, p, "Mode", opal_modes[mode > 4 ? 3 : mode], false);

    const char* usage_str = opal_usages[usage > 12 ? 13 : usage];
    if(usage_str) {
        metroflip_card_view_add_field(view, p, "Action", usage_str, false);
    }

    DateTime ts;
    opal_days_minutes_to_datetime(opal->days, opal->minutes, &ts);
    FuriString* ds = furi_string_alloc();
    locale_format_date(ds, &ts, locale_get_date_format(), "-");
    FuriString* ts_str = furi_string_alloc();
    locale_format_time(ts_str, &ts, locale_get_time_format(), false);
    snprintf(val, sizeof(val), "%s %s", furi_string_get_cstr(ds), furi_string_get_cstr(ts_str));
    furi_string_free(ds);
    furi_string_free(ts_str);
    metroflip_card_view_add_field(view, p, "When", val, false);

    snprintf(val, sizeof(val), "%u", opal->weekly_journeys);
    metroflip_card_view_add_field(view, p, "Wk Trips", val, false);

    snprintf(val, sizeof(val), "%u", opal->txn_number);
    metroflip_card_view_add_field(view, p, "Txn Number", val, false);

    /* Buttons */
    if(from_file) {
        metroflip_card_view_set_delete(view, true);
    } else {
        metroflip_card_view_set_save(view, true);
    }

    metroflip_card_view_show(app);
    return true;
}

static NfcCommand opal_poller_callback(NfcGenericEvent event, void* context) {
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

static void opal_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    if(app->data_loaded) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        if(flipper_format_file_open_existing(ff, app->file_path)) {
            MfDesfireData* data = mf_desfire_alloc();
            mf_desfire_load(data, ff, 2);

            if(!opal_display_card_view(data, app, true)) {
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
        nfc_poller_start(app->poller, opal_poller_callback, app);

        metroflip_app_blink_start(app);
    }
}

static bool opal_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipCustomEventPollerSuccess) {
            /* Read finished on the worker thread; build the card view here on
               the main/GUI thread. */
            metroflip_app_blink_stop(app);
            const MfDesfireData* data = nfc_device_get_data(app->nfc_device, NfcProtocolMfDesfire);
            if(!opal_display_card_view(data, app, false)) {
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

static void opal_on_exit(Metroflip* app) {
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
static const MetroflipPlugin opal_plugin = {
    .card_name = "Opal",
    .plugin_on_enter = opal_on_enter,
    .plugin_on_event = opal_on_event,
    .plugin_on_exit = opal_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor opal_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &opal_plugin,
};

/* Plugin entry point - must return a pointer to const descriptor  */
const FlipperAppPluginDescriptor* opal_plugin_ep(void) {
    return &opal_plugin_descriptor;
}
