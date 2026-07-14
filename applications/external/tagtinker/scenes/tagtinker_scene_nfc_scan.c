/*
 * NFC Scan scene — scan an ESL NFC tag to fill barcode
 */

#include "../tagtinker_app.h"

enum {
    NfcScanEventSuccess = 1,
    NfcScanEventNotEsl = 2,
};

static int32_t nfc_scan_thread(void* ctx) {
    TagTinkerApp* app = ctx;

    while(app->nfc_scanning) {
        MfUltralightData* mfu_data = mf_ultralight_alloc();
        MfUltralightError err = mf_ultralight_poller_sync_read_card(app->nfc, mfu_data, NULL);

        if(err == MfUltralightErrorNone) {
            char barcode[18];
            bool decoded = tagtinker_nfc_decode_barcode(mfu_data, barcode);
            mf_ultralight_free(mfu_data);

            if(!app->nfc_scanning) return 0;

            if(decoded) {
                memcpy(app->barcode, barcode, TAGTINKER_BC_LEN);
                app->barcode[TAGTINKER_BC_LEN] = '\0';
                view_dispatcher_send_custom_event(app->view_dispatcher, NfcScanEventSuccess);
            } else {
                view_dispatcher_send_custom_event(app->view_dispatcher, NfcScanEventNotEsl);
            }
            return 0;
        }

        mf_ultralight_free(mfu_data);

        if(!app->nfc_scanning) break;
        furi_delay_ms(100);
    }

    return 0;
}

void tagtinker_scene_nfc_scan_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Scan NFC Tag", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Hold ESL tag\nto Flipper back", 64, 32, AlignCenter, AlignCenter);

    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);

    notification_message(app->notifications, &sequence_blink_start_cyan);

    app->nfc = nfc_alloc();
    app->nfc_scanning = true;

    furi_thread_set_callback(app->nfc_thread, nfc_scan_thread);
    furi_thread_set_context(app->nfc_thread, app);
    furi_thread_start(app->nfc_thread);
}

bool tagtinker_scene_nfc_scan_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == NfcScanEventSuccess) {
        int8_t idx = tagtinker_ensure_target(app, app->barcode);

        if(idx < 0) {
            popup_reset(app->popup);
            popup_set_header(app->popup, "Decode Error", 64, 20, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, "Tag read but\nbarcode invalid", 64, 36, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 2000);
            popup_enable_timeout(app->popup);
            popup_set_callback(app->popup, NULL);
            view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
            return true;
        }

        tagtinker_select_target(app, (uint8_t)idx);

        FURI_LOG_I(
            TAGTINKER_TAG,
            "NFC: %s -> PLID %02X%02X%02X%02X",
            app->barcode,
            app->plid[3],
            app->plid[2],
            app->plid[1],
            app->plid[0]);

        notification_message(app->notifications, &sequence_success);
        scene_manager_next_scene(app->scene_manager, TagTinkerSceneTargetActions);
        return true;
    }

    if(event.event == NfcScanEventNotEsl) {
        popup_reset(app->popup);
        popup_set_header(app->popup, "Not an ESL tag", 64, 20, AlignCenter, AlignCenter);
        popup_set_text(
            app->popup, "Tag detected but\nno valid ESL data", 64, 36, AlignCenter, AlignCenter);
        popup_set_timeout(app->popup, 2000);
        popup_enable_timeout(app->popup);
        popup_set_callback(app->popup, NULL);
        view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
        return true;
    }

    return false;
}

void tagtinker_scene_nfc_scan_on_exit(void* ctx) {
    TagTinkerApp* app = ctx;

    app->nfc_scanning = false;

    if(app->nfc) {
        furi_thread_join(app->nfc_thread);
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
