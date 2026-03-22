#include "../wi_fir.h"
#include "../wfr_nfc.h"

/* Override popup input so back button reaches ViewDispatcher navigation callback */
static bool wi_fir_scan_nfc_input_cb(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

static NfcCommand wi_fir_nfc_poller_callback(NfcGenericEvent event, void* context) {
    WiFirApp* app = context;
    MfUltralightPollerEvent* mfu_event = event.event_data;

    switch(mfu_event->type) {
    case MfUltralightPollerEventTypeRequestMode:
        mfu_event->data->poller_mode = MfUltralightPollerModeRead;
        return NfcCommandContinue;

    case MfUltralightPollerEventTypeAuthRequest:
        mfu_event->data->auth_context.skip_auth = true;
        return NfcCommandContinue;

    case MfUltralightPollerEventTypeReadSuccess: {
        const MfUltralightData* mfu_data =
            (const MfUltralightData*)nfc_poller_get_data(app->nfc_poller);

        bool found = wfr_nfc_parse_wifi_tag(
            (const uint8_t*)mfu_data->page, mfu_data->pages_read, &app->nfc_creds);

        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            found ? WiFirCustomEventNfcWifiFound : WiFirCustomEventNfcNotWifi);
        return NfcCommandStop;
    }

    case MfUltralightPollerEventTypeReadFailed:
        /* Don't give up — reset and let the poller try again.
         * This handles the case where the tag wasn't positioned yet
         * or communication was briefly interrupted. */
        return NfcCommandReset;

    default:
        return NfcCommandContinue;
    }
}

void wi_fir_scene_scan_nfc_on_enter(void* context) {
    WiFirApp* app = context;

    /* Show scanning popup */
    popup_reset(app->popup);
    popup_set_header(app->popup, "Scanning...", 64, 10, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        "Place NFC WiFi tag\non back of Flipper",
        64,
        35,
        AlignCenter,
        AlignCenter);
    /* Override popup input so back button reaches scene manager */
    view_set_input_callback(popup_get_view(app->popup), wi_fir_scan_nfc_input_cb);

    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);

    /* Start NFC detection LED blink (cyan like the stock NFC app) */
    notification_message(app->notifications, &sequence_blink_start_cyan);

    /* Start NFC poller for MfUltralight (covers NTAG213/215/216) */
    app->nfc = nfc_alloc();
    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->nfc_poller, wi_fir_nfc_poller_callback, app);
}

bool wi_fir_scene_scan_nfc_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventNfcWifiFound) {
            /* Copy NFC credentials to working set */
            strncpy(app->ssid, app->nfc_creds.ssid, WFR_SSID_MAX_LEN);
            strncpy(app->password, app->nfc_creds.password, WFR_PASS_MAX_LEN);
            app->security_type = app->nfc_creds.security;
            app->hidden = app->nfc_creds.hidden;

            notification_message(app->notifications, &sequence_success);

            /* Go straight to confirm (not from saved, so clear delete context) */
            app->selected_saved_file[0] = '\0';
            scene_manager_next_scene(app->scene_manager, WiFirSceneConfirm);
            consumed = true;

        } else if(event.event == WiFirCustomEventNfcNotWifi) {
            notification_message(app->notifications, &sequence_error);

            popup_reset(app->popup);
            popup_set_header(app->popup, "Not a WiFi Tag", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "No WiFi credentials\nfound on this tag", 64, 40, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 2000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;

        } else if(event.event == WiFirCustomEventNfcError) {
            notification_message(app->notifications, &sequence_error);

            popup_reset(app->popup);
            popup_set_header(app->popup, "Read Error", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "Failed to read tag\nTry again", 64, 40, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 2000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void wi_fir_scene_scan_nfc_on_exit(void* context) {
    WiFirApp* app = context;

    /* Stop LED blink */
    notification_message(app->notifications, &sequence_blink_stop);

    /* Stop and free NFC resources */
    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    popup_reset(app->popup);
}
