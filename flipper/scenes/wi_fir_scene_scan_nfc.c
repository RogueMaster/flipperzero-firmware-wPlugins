#include "../wi_fir.h"
#include "../wfr_nfc.h"

#define NFC_ERROR_TICKS 20 /* show the error for 2s, then scan again */

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

static void wi_fir_nfc_stop(WiFirApp* app) {
    if(app->nfc_poller) {
        nfc_poller_stop(app->nfc_poller);
        nfc_poller_free(app->nfc_poller);
        app->nfc_poller = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }
}

static void wi_fir_nfc_start(WiFirApp* app) {
    wi_fir_nfc_stop(app);
    app->dismiss_ticks = 0;
    wfr_status_view_set_nfc_scan(app->status);

    /* Cyan LED blink like the stock NFC app */
    notification_message(app->notifications, &sequence_blink_start_cyan);

    /* Poll for MfUltralight (covers NTAG213/215/216) */
    app->nfc = nfc_alloc();
    app->nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start(app->nfc_poller, wi_fir_nfc_poller_callback, app);
}

static void wi_fir_nfc_show_error(WiFirApp* app, const char* title, const char* line1, const char* line2) {
    wi_fir_nfc_stop(app);
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_error);
    wfr_status_view_set_result(
        app->status, &I_menu_nfc_20x16, WfrStatusBadgeCross, title, line1, line2, NULL, NULL);
    app->dismiss_ticks = NFC_ERROR_TICKS;
}

static void wi_fir_nfc_status_callback(void* context, InputKey key) {
    WiFirApp* app = context;
    if(key == InputKeyLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, WiFirCustomEventStatusLeft);
    }
}

void wi_fir_scene_scan_nfc_on_enter(void* context) {
    WiFirApp* app = context;
    wfr_status_view_set_callback(app->status, wi_fir_nfc_status_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewStatus);
    wi_fir_nfc_start(app);
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
            wi_fir_nfc_show_error(app, "Not Wi-Fi", "No credentials", "on this tag");
            consumed = true;

        } else if(event.event == WiFirCustomEventNfcError) {
            wi_fir_nfc_show_error(app, "Read error", "Couldn't read tag", "Hold it still");
            consumed = true;

        } else if(event.event == WiFirCustomEventStatusLeft && app->dismiss_ticks == 0) {
            /* Left on the scan screen matches its "Back" hint */
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeTick) {
        if(app->dismiss_ticks > 0 && --app->dismiss_ticks == 0) {
            wi_fir_nfc_start(app);
        }
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void wi_fir_scene_scan_nfc_on_exit(void* context) {
    WiFirApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    wi_fir_nfc_stop(app);
    app->dismiss_ticks = 0;
}
