#include <dolphin/dolphin.h>
#include <lib/nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <lib/bit_lib/bit_lib.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include "../../metroflip_i.h"
#include "../../metroflip_plugins.h"
#include "../../api/metroflip/metroflip_api.h"

#define TAG "Metroflip:Scene:ATR_CHECK"

static void delay(int milliseconds) {
    furi_thread_flags_wait(0, FuriFlagWaitAny, milliseconds);
}

static NfcCommand
    atr_poller_callback_iso14443_4a(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolIso14443_4a);

    Metroflip* app = context;
    const Iso14443_4aPollerEvent* iso14443_4a_event = event.event_data;

    if(iso14443_4a_event->type == Iso14443_4aPollerEventTypeReady) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolIso14443_4a, nfc_poller_get_data(app->poller));
        view_dispatcher_send_custom_event(app->view_dispatcher, MetroflipPollerEventTypeSuccess);
        return NfcCommandStop;
    }

    return NfcCommandContinue;
}

static void atr_on_enter(Metroflip* app) {
    dolphin_deed(DolphinDeedNfcRead);

    // Setup view
    Popup* popup = app->popup;
    popup_set_header(popup, "Parsing...", 68, 30, AlignLeft, AlignTop);
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    // Start worker
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(app->poller, atr_poller_callback_iso14443_4a, app);

    if(!app->data_loaded) {
        popup_reset(app->popup);
        popup_set_header(popup, "Apply\n card to\nthe back", 68, 30, AlignLeft, AlignTop);
        view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
        metroflip_app_blink_start(app);
    }
}

static bool atr_on_event(Metroflip* app, SceneManagerEvent event) {
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MetroflipPollerEventTypeSuccess) {
            NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notification, &sequence_set_vibro_on);
            delay(50);
            notification_message(notification, &sequence_reset_vibro);
            const Iso14443_4aData* data = nfc_device_get_data(app->nfc_device, NfcProtocolIso14443_4a);
            uint32_t hist_bytes_count;
            const uint8_t* hist_bytes = iso14443_4a_get_historical_bytes(data, &hist_bytes_count);
            FURI_LOG_I(TAG, "Historical bytes count: %ld", hist_bytes_count);
            if(hist_bytes && hist_bytes_count > 0) {
                memcpy(app->hist_bytes, hist_bytes, MIN(hist_bytes_count, sizeof(app->hist_bytes)));
                app->hist_bytes_count = MIN(hist_bytes_count, sizeof(app->hist_bytes));
            }
            scene_manager_next_scene(app->scene_manager, MetroflipSceneParse);
            consumed = true;
        } else if(event.event == MetroflipPollerEventTypeCardDetect) {
            Popup* popup = app->popup;
            popup_set_header(popup, "Scanning..", 68, 30, AlignLeft, AlignTop);
            consumed = true;
        }else if(event.event == MetroflipCustomEventPollerFileNotFound) {
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

static void atr_on_exit(Metroflip* app) {
    widget_reset(app->widget);
    metroflip_app_blink_stop(app);

    if(app->poller && !app->data_loaded) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
    }
}

static const MetroflipPlugin atr_plugin = {
    .card_name = "ATR",
    .plugin_on_enter = atr_on_enter,
    .plugin_on_event = atr_on_event,
    .plugin_on_exit = atr_on_exit,
};

/* Plugin descriptor to comply with basic plugin specification */
static const FlipperAppPluginDescriptor atr_plugin_descriptor = {
    .appid = METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
    .ep_api_version = METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
    .entry_point = &atr_plugin,
};

const FlipperAppPluginDescriptor* atr_plugin_ep(void) {
    return &atr_plugin_descriptor;
}