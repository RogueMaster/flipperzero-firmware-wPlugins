#include "../wi_fir.h"
#include "../wfr_encode.h"
#include "../wfr_decode.h"
#include "../wfr_storage.h"
#include <infrared_worker.h>
#include <notification/notification_messages.h>

#define TAG "InfraFiTX"

/* IR receive callback — runs on IR worker thread */
static void wi_fir_ir_rx_callback(void* context, InfraredWorkerSignal* signal) {
    WiFirApp* app = context;

    if(!infrared_worker_signal_is_decoded(signal)) return;

    const InfraredMessage* message = infrared_worker_get_decoded_signal(signal);
    if(message->protocol != InfraredProtocolRC6) return;

    uint8_t address = (uint8_t)(message->address & 0xFF);
    uint8_t command = (uint8_t)(message->command & 0xFF);

    char payload[WFR_MAX_TOTAL_PAYLOAD + 1];
    int result = wfr_ack_decode_feed(
        &app->ack_decoder, address, command, payload, sizeof(payload));

    if(result > 0) {
        if(strncmp(payload, WFR_ACK_PREFIX_OK, strlen(WFR_ACK_PREFIX_OK)) == 0) {
            const char* ip = payload + strlen(WFR_ACK_PREFIX_OK);
            strncpy(app->ack_result_text, ip, sizeof(app->ack_result_text) - 1);
            app->ack_result_text[sizeof(app->ack_result_text) - 1] = '\0';
            view_dispatcher_send_custom_event(
                app->view_dispatcher, WiFirCustomEventAckSuccess);
        } else {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, WiFirCustomEventAckFail);
        }
    }
}

/* ACK timeout — no response from server within WFR_ACK_TIMEOUT_SEC */
static void wi_fir_ack_timeout_callback(void* context) {
    WiFirApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, WiFirCustomEventAckTimeout);
}

/* Auto-dismiss result popup back to main menu */
static void wi_fir_result_popup_callback(void* context) {
    WiFirApp* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, WiFirCustomEventAckDismiss);
}

/* Stop ACK listener (IR worker + timer) — safe to call multiple times */
static void wi_fir_stop_ack_listener(WiFirApp* app) {
    if(app->ack_timer) {
        furi_timer_stop(app->ack_timer);
        furi_timer_free(app->ack_timer);
        app->ack_timer = NULL;
    }
    if(app->ir_worker) {
        infrared_worker_rx_stop(app->ir_worker);
        infrared_worker_free(app->ir_worker);
        app->ir_worker = NULL;
    }
}

void wi_fir_scene_transmit_on_enter(void* context) {
    WiFirApp* app = context;

    /* Show loading animation while transmitting */
    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewLoading);

    /* Build credentials struct */
    WfrWifiCreds creds;
    memset(&creds, 0, sizeof(creds));
    strncpy(creds.ssid, app->ssid, WFR_SSID_MAX_LEN);
    strncpy(creds.password, app->password, WFR_PASS_MAX_LEN);
    creds.security = app->security_type;
    creds.hidden = app->hidden;

    /* Blink LED during transmission */
    notification_message(app->notifications, &sequence_blink_start_magenta);

    /* Transmit via IR */
    bool success = wfr_transmit_credentials(&creds);

    notification_message(app->notifications, &sequence_blink_stop);

    if(!success) {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventTransmitFail);
        return;
    }

    /* Auto-save credentials to SD card */
    wfr_storage_save(app->storage, &creds);
    notification_message(app->notifications, &sequence_success);

    if(!app->ack_enabled) {
        /* ACK disabled — show simple "Sent!" popup and return */
        view_dispatcher_send_custom_event(
            app->view_dispatcher, WiFirCustomEventTransmitDone);
        return;
    }

    /* Start listening for ACK from server */
    FURI_LOG_I(TAG, "Starting ACK listener");
    wfr_ack_decode_init(&app->ack_decoder);
    app->ack_result_text[0] = '\0';

    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_enable_signal_decoding(app->ir_worker, true);
    infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, true);
    infrared_worker_rx_set_received_signal_callback(
        app->ir_worker, wi_fir_ir_rx_callback, app);
    infrared_worker_rx_start(app->ir_worker);

    /* Timeout timer */
    app->ack_timer = furi_timer_alloc(
        wi_fir_ack_timeout_callback, FuriTimerTypeOnce, app);
    furi_timer_start(app->ack_timer, furi_ms_to_ticks(WFR_ACK_TIMEOUT_SEC * 1000));

    /* Show "waiting" popup with cyan LED pulse */
    notification_message(app->notifications, &sequence_blink_start_cyan);
    popup_reset(app->popup);
    popup_set_header(app->popup, "Sent!", 64, 10, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        "Waiting for server\nresponse...",
        64, 35, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
}

bool wi_fir_scene_transmit_on_event(void* context, SceneManagerEvent event) {
    WiFirApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == WiFirCustomEventAckSuccess) {
            wi_fir_stop_ack_listener(app);
            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_success);

            popup_reset(app->popup);
            popup_set_header(app->popup, "Connected!", 64, 10, AlignCenter, AlignCenter);
            snprintf(
                app->confirm_text, sizeof(app->confirm_text),
                "IP: %s", app->ack_result_text);
            popup_set_text(
                app->popup, app->confirm_text, 64, 35, AlignCenter, AlignCenter);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, wi_fir_result_popup_callback);
            popup_set_timeout(app->popup, 5000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;

        } else if(event.event == WiFirCustomEventAckFail) {
            wi_fir_stop_ack_listener(app);
            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_error);

            popup_reset(app->popup);
            popup_set_header(app->popup, "Failed", 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                "Server could not\nconnect to WiFi",
                64, 35, AlignCenter, AlignCenter);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, wi_fir_result_popup_callback);
            popup_set_timeout(app->popup, 3000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;

        } else if(event.event == WiFirCustomEventAckTimeout) {
            wi_fir_stop_ack_listener(app);
            notification_message(app->notifications, &sequence_blink_stop);

            popup_reset(app->popup);
            popup_set_header(app->popup, "Sent!", 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                "No response from server\n(credentials were sent)",
                64, 35, AlignCenter, AlignCenter);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, wi_fir_result_popup_callback);
            popup_set_timeout(app->popup, 3000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;

        } else if(event.event == WiFirCustomEventTransmitDone) {
            /* ACK disabled — simple success popup */
            popup_reset(app->popup);
            popup_set_header(app->popup, "Sent!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                "Credentials transmitted\nvia IR",
                64, 40, AlignCenter, AlignCenter);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, wi_fir_result_popup_callback);
            popup_set_timeout(app->popup, 2000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;

        } else if(event.event == WiFirCustomEventTransmitFail) {
            popup_reset(app->popup);
            popup_set_header(app->popup, "Error!", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "IR transmission failed", 64, 40, AlignCenter, AlignCenter);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, wi_fir_result_popup_callback);
            popup_set_timeout(app->popup, 2000);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, WiFirViewPopup);
            consumed = true;

        } else if(event.event == WiFirCustomEventAckDismiss) {
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, WiFirSceneMainMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, WiFirSceneMainMenu);
        consumed = true;
    }

    return consumed;
}

void wi_fir_scene_transmit_on_exit(void* context) {
    WiFirApp* app = context;
    wi_fir_stop_ack_listener(app);
    popup_reset(app->popup);
    notification_message(app->notifications, &sequence_blink_stop);
}
