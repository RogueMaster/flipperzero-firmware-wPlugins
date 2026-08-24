#include "nfc_login_scan.h"
#include "../scenes/enroll/enroll_scene.h"
#include "../hid/nfc_login_hid.h"
#include "../hid/nfc_login_hid_ble.h"
#include "../settings/nfc_login_notify.h"
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>

static void app_scan_handle_uid(App* app, const uint8_t* uid, size_t uid_len) {
    int match_index = -1;
    if(app->has_active_selection && app->active_card_index < app->card_count) {
        if(app->cards[app->active_card_index].uid_len == uid_len &&
           memcmp(app->cards[app->active_card_index].uid, uid, uid_len) == 0) {
            match_index = (int)app->active_card_index;
        }
    } else {
        for(size_t i = 0; i < app->card_count; i++) {
            if(app->cards[i].uid_len == uid_len && memcmp(app->cards[i].uid, uid, uid_len) == 0) {
                match_index = (int)i;
                break;
            }
        }
    }

    if(match_index < 0) {
        if(app->has_active_selection) {
            nfc_login_notify(app, NfcLoginNotifyError);
            furi_delay_ms(ERROR_NOTIFICATION_DELAY_MS);
        }
        return;
    }

    if(!app->scanning) return;

    nfc_login_notify(app, NfcLoginNotifySuccess);

    HidMode effective_mode = app->hid_mode;

#if !HAS_BLE_HID_API
    if(effective_mode == HidModeBle) {
        effective_mode = HidModeUsb;
    }
#endif

    if(effective_mode == HidModeUsb) {
        app->previous_usb_config = furi_hal_usb_get_config();
    } else {
        app->previous_usb_config = NULL;
    }

    bool hid_ready = false;
    if(effective_mode == HidModeBle) {
#if HAS_BLE_HID_API
        hid_ready = is_ble_hid_ready();
        if(!hid_ready) {
            hid_ready = ble_hid_init();
            if(hid_ready) {
                uint8_t retries = 100;
                for(uint8_t i = 0; i < retries && !ble_hid_is_connected(); i++) {
                    furi_delay_ms(BLE_CONNECTION_RETRY_DELAY_MS);
                }
            }
        }
#else
        hid_ready = false;
#endif
    } else {
        hid_ready = initialize_hid_and_wait_with_mode(effective_mode);
    }

    if(hid_ready) {
        if(!app->scanning) return;

        furi_delay_ms(HID_POST_CONNECT_DELAY_MS);
        if(!app->scanning) return;

        release_all_keys_with_mode(effective_mode);
        app_type_password(app, app->cards[match_index].password);

        if(!app->scanning) return;

        if(effective_mode == HidModeUsb) {
            deinitialize_hid_with_restore_and_mode(app->previous_usb_config, effective_mode);
            app->previous_usb_config = NULL;
        }
        furi_delay_ms(HID_POST_TYPE_DELAY_MS);
    } else {
        nfc_login_notify(app, NfcLoginNotifyError);
    }
}

/* One poller attempt. wait_ms = FuriWaitForever for continuous, else pulse remainder. */
static bool app_scan_once(App* app, Nfc* nfc, AsyncPollerContext* async_ctx, uint32_t wait_ms) {
    async_ctx->detected = false;
    async_ctx->error = Iso14443_3aErrorNone;

    NfcPoller* poller = nfc_poller_alloc(nfc, NfcProtocolIso14443_3a);
    async_ctx->poller = poller;
    nfc_poller_start(poller, iso14443_3a_async_callback, async_ctx);

    const uint32_t flags =
        furi_thread_flags_wait(ISO14443_3A_ASYNC_FLAG_COMPLETE, FuriFlagWaitAny, wait_ms);
    furi_thread_flags_clear(ISO14443_3A_ASYNC_FLAG_COMPLETE);

    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    async_ctx->poller = NULL;

    if((flags & FuriFlagError) != 0) {
        return false;
    }
    if(!(flags & ISO14443_3A_ASYNC_FLAG_COMPLETE)) {
        return false;
    }
    if(!async_ctx->detected || async_ctx->error != Iso14443_3aErrorNone) {
        return false;
    }

    size_t uid_len = 0;
    const uint8_t* uid = iso14443_3a_get_uid(&async_ctx->iso14443_3a_data, &uid_len);
    if(!uid || uid_len == 0) {
        return false;
    }

    app_scan_handle_uid(app, uid, uid_len);
    return true;
}

int32_t app_scan_thread(void* context) {
    App* app = context;

    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        FURI_LOG_E(TAG, "Failed to allocate NFC");
        return 0;
    }

    AsyncPollerContext async_ctx = {
        .thread_id = furi_thread_get_current_id(),
        .reset_counter = 0,
        .detected = false,
        .error = Iso14443_3aErrorNone,
        .poller = NULL,
    };

    while(app->scanning) {
        if(!app->active_scan) {
            while(app->scanning && !app->scan_pulse) {
                furi_delay_ms(50);
            }
            if(!app->scanning) break;
            app->scan_pulse = false;

            const uint32_t pulse_end = furi_get_tick() + NFC_SCAN_PULSE_MS;
            while(app->scanning && furi_get_tick() < pulse_end) {
                uint32_t left = pulse_end - furi_get_tick();
                if(left == 0) break;
                if(app_scan_once(app, nfc, &async_ctx, left)) {
                    break;
                }
                furi_delay_ms(NFC_COOLDOWN_DELAY_MS);
            }

            if(app->scanning && app->view_dispatcher) {
                view_dispatcher_send_custom_event(app->view_dispatcher, EventScanPulseDone);
            }
            continue;
        }

        app_scan_once(app, nfc, &async_ctx, FuriWaitForever);
        if(!app->scanning) break;
        furi_delay_ms(NFC_COOLDOWN_DELAY_MS);
        furi_delay_ms(NFC_SCAN_DELAY_MS);
    }

    nfc_free(nfc);
    return 0;
}
