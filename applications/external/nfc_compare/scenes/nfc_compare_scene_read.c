#include "../nfc_compare.h"

/* Runs on the NfcPoller worker thread. Keeps polling until an ISO14443-3A card
 * is successfully activated, then latches its UID and asks the UI thread to
 * advance to the result scene. No manual radio state handling. */
static NfcCommand nfc_compare_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    furi_assert(event.protocol == NfcProtocolIso14443_3a);
    NfcCompareApp* app = context;

    const Iso14443_3aPollerEvent* iso_event = event.event_data;

    if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
        const Iso14443_3aData* data = nfc_poller_get_data(app->poller);
        size_t len = 0;
        const uint8_t* uid = iso14443_3a_get_uid(data, &len);
        if(len > NFC_COMPARE_UID_MAX_LEN) len = NFC_COMPARE_UID_MAX_LEN;
        if(uid && len > 0) {
            memcpy(app->read_uid, uid, len);
        }
        app->read_uid_len = len;
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcCompareCustomEventCardDetected);
        return NfcCommandStop;
    }

    // Activation error (e.g. card removed mid-read): keep trying.
    return NfcCommandContinue;
}

void nfc_compare_scene_read_on_enter(void* context) {
    furi_assert(context);
    NfcCompareApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 6, AlignCenter, AlignTop, FontPrimary, "NFC Compare");
    widget_add_string_multiline_element(
        app->widget,
        64,
        34,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        "Apply the card\nto the back of Flipper");
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcCompareViewWidget);

    app->read_uid_len = 0;
    app->nfc = nfc_alloc();
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
    nfc_poller_start(app->poller, nfc_compare_poller_callback, app);
}

bool nfc_compare_scene_read_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NfcCompareApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcCompareCustomEventCardDetected) {
            scene_manager_next_scene(app->scene_manager, NfcCompareSceneResult);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_compare_scene_read_on_exit(void* context) {
    furi_assert(context);
    NfcCompareApp* app = context;

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }
    widget_reset(app->widget);
}
