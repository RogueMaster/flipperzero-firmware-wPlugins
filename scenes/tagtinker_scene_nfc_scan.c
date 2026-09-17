/*
 * NFC Scan scene — read an ESL's NFC tag to fill the barcode
 *
 * The scanner keeps running after a tag is rejected, so an unsupported tag no
 * longer ends the session: present another one and it is read straight away.
 * Readable tags are de-duplicated by UID, and an unreadable chip is announced
 * once, so a tag left resting on the reader does not re-trigger its message
 * every poll.
 *
 * The messages only describe the NFC data. The app cannot sense whether a
 * label's display is driven by infrared or radio.
 */

#include "../tagtinker_app.h"
#include <string.h>

/* How long a rejection message stays before the prompt returns. */
#define NFC_SCAN_MESSAGE_MS 2500U

/* Custom event ids. After Back, an event queued just before it is delivered to
 * the target menu, which uses 0..15 for saved targets and 99/100 for its add
 * items, so keep these well clear of both. */
enum {
    NfcScanEventSuccess = 200,
    NfcScanEventUnrecognized = 201,
    NfcScanEventImagotagLink = 202,
    NfcScanEventUnreadable = 203,
};

/* The prompt comes back on a scene tick, not through the popup callback: a
 * popup with a callback consumes every short key press, Back included, so
 * one Back would only dismiss the message instead of leaving the scene. */
static bool nfc_scan_rearm_pending = false;
static uint32_t nfc_scan_rearm_since = 0;

static void nfc_scan_show_prompt(TagTinkerApp* app) {
    nfc_scan_rearm_pending = false;
    popup_reset(app->popup);
    popup_disable_timeout(app->popup);
    popup_set_callback(app->popup, NULL);
    popup_set_header(app->popup, "Scan NFC Tag", 64, 10, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Hold ESL tag\nto Flipper back", 64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
}

/* Show a transient message. When rearm is set the scanner is still running, so
 * the prompt comes back by itself; otherwise the message stays until Back. */
static void
    nfc_scan_show_message(TagTinkerApp* app, const char* header, const char* text, bool rearm) {
    popup_reset(app->popup);
    popup_set_header(app->popup, header, 64, 20, AlignCenter, AlignCenter);
    popup_set_text(app->popup, text, 64, 38, AlignCenter, AlignCenter);
    popup_set_callback(app->popup, NULL);
    popup_disable_timeout(app->popup);
    nfc_scan_rearm_pending = rearm;
    nfc_scan_rearm_since = furi_get_tick();
    view_dispatcher_switch_to_view(app->view_dispatcher, TagTinkerViewPopup);
}

static int32_t nfc_scan_thread(void* ctx) {
    TagTinkerApp* app = ctx;
    uint8_t last_uid[10];
    uint8_t last_uid_len = 0;
    bool unreadable_announced = false;
    TagTinkerNfcResult last_result = TagTinkerNfcResultUnreadable;

    while(app->nfc_scanning) {
        MfUltralightData* mfu_data = mf_ultralight_alloc();
        MfUltralightError err = mf_ultralight_poller_sync_read_card(app->nfc, mfu_data, NULL);

        if(!app->nfc_scanning) {
            mf_ultralight_free(mfu_data);
            break;
        }

        if(err != MfUltralightErrorNone) {
            /* No chip was activated, or the poller failed before reading one.
             * Treat the field as empty so the next tag is announced. */
            last_uid_len = 0;
            unreadable_announced = false;
            mf_ultralight_free(mfu_data);
            furi_delay_ms(100);
            continue;
        }

        char barcode[18];
        TagTinkerNfcResult result = tagtinker_nfc_classify(mfu_data, barcode);
        bool repeat = false;

        if(result == TagTinkerNfcResultUnreadable) {
            /* An unreadable read carries no UID, so it cannot be de-duplicated
             * by UID. Announce it once until the field is empty again. */
            repeat = unreadable_announced;
            unreadable_announced = true;
        } else {
            uint8_t uid_len = mfu_data->iso14443_3a_data->uid_len;
            if(uid_len > sizeof(last_uid)) uid_len = sizeof(last_uid);
            const uint8_t* uid = mfu_data->iso14443_3a_data->uid;

            /* Compare the result too: a read that stopped partway comes back
             * as a success with fewer pages, and a later full read of the
             * same tag must still be able to decode it. */
            repeat = uid_len > 0 && uid_len == last_uid_len &&
                     memcmp(uid, last_uid, uid_len) == 0 && result == last_result;
            memcpy(last_uid, uid, uid_len);
            last_uid_len = uid_len;
            last_result = result;
        }

        mf_ultralight_free(mfu_data);

        if(repeat) {
            /* Same tag still resting on the reader; do not announce it again. */
            furi_delay_ms(100);
            continue;
        }

        uint32_t event = NfcScanEventUnrecognized;
        switch(result) {
        case TagTinkerNfcResultDecoded:
            memcpy(app->barcode, barcode, TAGTINKER_BC_LEN);
            app->barcode[TAGTINKER_BC_LEN] = '\0';
            event = NfcScanEventSuccess;
            break;
        case TagTinkerNfcResultImagotagLink:
            event = NfcScanEventImagotagLink;
            break;
        case TagTinkerNfcResultUnreadable:
            event = NfcScanEventUnreadable;
            break;
        case TagTinkerNfcResultUnrecognized:
            event = NfcScanEventUnrecognized;
            break;
        }

        if(!app->nfc_scanning) break;
        view_dispatcher_send_custom_event(app->view_dispatcher, event);

        /* On success the scene moves on; stop polling so the field is not left
         * running underneath the next scene. */
        if(event == NfcScanEventSuccess) return 0;

        furi_delay_ms(100);
    }

    return 0;
}

void tagtinker_scene_nfc_scan_on_enter(void* ctx) {
    TagTinkerApp* app = ctx;

    nfc_scan_show_prompt(app);

    notification_message(app->notifications, &sequence_blink_start_cyan);

    app->nfc = nfc_alloc();
    app->nfc_scanning = true;

    furi_thread_set_callback(app->nfc_thread, nfc_scan_thread);
    furi_thread_set_context(app->nfc_thread, app);
    furi_thread_start(app->nfc_thread);
}

bool tagtinker_scene_nfc_scan_on_event(void* ctx, SceneManagerEvent event) {
    TagTinkerApp* app = ctx;

    if(event.type == SceneManagerEventTypeBack) {
        /* Stop the scanner as soon as Back is pressed rather than waiting for
         * on_exit, so the poll loop is already winding down when the scene is
         * popped and the join there is as short as possible. Returning false
         * lets the scene manager pop exactly one scene, as before. */
        app->nfc_scanning = false;
        nfc_scan_rearm_pending = false;
        return false;
    }

    if(event.type == SceneManagerEventTypeTick) {
        if(nfc_scan_rearm_pending &&
           furi_get_tick() - nfc_scan_rearm_since >= furi_ms_to_ticks(NFC_SCAN_MESSAGE_MS)) {
            nfc_scan_show_prompt(app);
        }
        return false;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == NfcScanEventSuccess) {
        int8_t idx = tagtinker_ensure_target(app, app->barcode);

        if(idx < 0) {
            /* The barcode came out of the decoder, so it always parses; a
             * failure here means there is no free target slot left. The scan
             * thread has already stopped, so stop the scan LED too. */
            notification_message(app->notifications, &sequence_blink_stop);
            nfc_scan_show_message(
                app, "Target list full", "Delete a target\nand scan again", false);
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

    if(event.event == NfcScanEventImagotagLink) {
        /* The link points at nfc.imagotag.com, the host in the public VUSION
         * label dump (i12bp8/TagTinker#51). SES-imagotag's VUSION access point
         * datasheet documents a proprietary 2.4 GHz label radio, and this app
         * only transmits infrared. The link alone does not prove the model or
         * its radio, hence "Likely". */
        nfc_scan_show_message(
            app, "Likely radio tag", "Link: nfc.imagotag.com\nTagTinker is IR-only", true);
        return true;
    }

    if(event.event == NfcScanEventUnrecognized) {
        nfc_scan_show_message(app, "Unrecognized tag", "No ID TagTinker\ncan decode", true);
        return true;
    }

    if(event.event == NfcScanEventUnreadable) {
        nfc_scan_show_message(app, "Unreadable chip", "Chip answered but\nno data was read", true);
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

    nfc_scan_rearm_pending = false;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
    popup_disable_timeout(app->popup);
    popup_set_callback(app->popup, NULL);
}
