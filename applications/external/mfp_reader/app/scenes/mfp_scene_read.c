#include "../mfp_app.h"
#include "mfp_scene_config.h"

#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <gui/scene_manager.h>
#include <string.h>
#include <stdio.h>

#include "mfp_reader_icons.h"

typedef enum {
    ReadEventSuccess = 0,
    ReadEventNotMfp,
} ReadCustomEvent;

static bool mfp_read_detect_from_ats(MfpApp* app, const uint8_t* ats, uint32_t ats_len) {
    if(!ats || ats_len < 3) return false;
    if(ats[0] != 0xC1) return false;

    uint8_t card_type = (ats[2] >> 4) & 0x0F;
    if(card_type != 0x02) return false;

    app->version.hw_vendor = 0x04;
    app->version.sl = MfpSL3;

    uint8_t size_code = ats[2] & 0x0F;
    app->version.size = (size_code >= 0x02) ? MfpSize4K : MfpSize2K;

    return true;
}

/* ---- NFC callback (NFC thread) — NO storage I/O here ---- */

static NfcCommand mfp_read_poller_cb(NfcGenericEvent event, void* ctx) {
    MfpApp* app = ctx;
    furi_assert(event.protocol == NfcProtocolIso14443_4a);

    const Iso14443_4aPollerEvent* ev = event.event_data;

    if(ev->type == Iso14443_4aPollerEventTypeError) {
        app->last_iso_error = ev->data ? (int)ev->data->error : -1;
        return NfcCommandContinue;
    }

    if(ev->type != Iso14443_4aPollerEventTypeReady) {
        return NfcCommandContinue;
    }

    const Iso14443_4aData* t4a = (const Iso14443_4aData*)nfc_poller_get_data(app->poller);
    if(t4a && t4a->iso14443_3a_data) {
        app->sak = t4a->iso14443_3a_data->sak;
        memcpy(app->atqa, t4a->iso14443_3a_data->atqa, 2);

        uint8_t uid_len = t4a->iso14443_3a_data->uid_len;
        if(uid_len > sizeof(app->version.uid)) uid_len = sizeof(app->version.uid);
        memcpy(app->version.uid, t4a->iso14443_3a_data->uid, uid_len);
        app->version.uid_len = uid_len;
    }

    if(t4a) {
        uint32_t ats_len = 0;
        const uint8_t* ats = iso14443_4a_get_historical_bytes(t4a, &ats_len);
        if(ats && ats_len > 0) {
            if(ats_len > sizeof(app->ats_bytes)) ats_len = sizeof(app->ats_bytes);
            memcpy(app->ats_bytes, ats, (size_t)ats_len);
            app->ats_len = (uint8_t)ats_len;
        }
    }

    app->last_error = mfp_poller_read_version(
        event.instance,
        &app->version,
        &app->last_iso_error,
        app->dbg_resp,
        &app->dbg_resp_len);

    if(app->last_error != MfpOk) {
        if(mfp_read_detect_from_ats(app, app->ats_bytes, app->ats_len)) {
            app->card_identified = true;
            view_dispatcher_send_custom_event(app->view_dispatcher, ReadEventSuccess);
            return NfcCommandStop;
        } else {
            app->card_identified = false;
            view_dispatcher_send_custom_event(app->view_dispatcher, ReadEventNotMfp);
            return NfcCommandStop;
        }
    }

    app->card_identified = true;
    view_dispatcher_send_custom_event(app->view_dispatcher, ReadEventSuccess);
    return NfcCommandStop;
}

/* ---- Scene ---- */

static void read_start_poller(MfpApp* app) {
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(app->poller, mfp_read_poller_cb, app);
}

static void read_stop_poller(MfpApp* app) {
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

void mfp_scene_read_on_enter(void* ctx) {
    MfpApp* app = ctx;

    app->card_identified = false;
    app->last_iso_error  = -1;
    memset(&app->version, 0, sizeof(app->version));
    app->sak     = 0;
    app->atqa[0] = 0;
    app->atqa[1] = 0;
    app->ats_len = 0;

    /* Clear any leftover scan state from a previous session so Card
     * Info correctly recognizes this as a fresh pre-dump preview. */
    memset(app->sector_results, 0, sizeof(app->sector_results));
    memset(app->blocks, 0, sizeof(app->blocks));
    app->blocks_read = 0;
    app->scan_sectors_ok = 0;
    app->scan_sectors_done = 0;
    app->scan_total_sectors = 0;
    app->read_all_mode = false;
    app->loaded_from_file = false;

    popup_reset(app->popup);
    popup_set_header(app->popup, "Reading", 97, 15, AlignCenter, AlignTop);
    popup_set_text(
        app->popup,
        "Hold card next\nto Flipper's back",
        94, 27, AlignCenter, AlignTop);
    popup_set_icon(app->popup, 0, 8, &I_nfc_manual_60x50);
    view_dispatcher_switch_to_view(app->view_dispatcher, MfpViewPopup);

    notification_message(app->notifications, &sequence_blink_start_blue);
    read_start_poller(app);
}

bool mfp_scene_read_on_event(void* ctx, SceneManagerEvent event) {
    MfpApp* app = ctx;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == ReadEventSuccess) {
        /* Stop blinking LED — on_exit is NOT called on forward navigation */
        notification_message(app->notifications, &sequence_blink_stop);
        notification_message(app->notifications, &sequence_success);
        read_stop_poller(app);
        furi_string_reset(app->dict_path);
        /* Show decoded card info first. The Card Info scene has a Dump
         * button that continues the flow into the dump dictionary choice. */
        scene_manager_next_scene(app->scene_manager, MfpSceneCardInfo);
        return true;
    }

    if(event.event == ReadEventNotMfp) {
        notification_message(app->notifications, &sequence_blink_stop);
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Read Failed", 90, 3, AlignCenter, AlignTop);
        if(app->last_error == MfpErrorComm) {
            popup_set_text(
                app->popup,
                "ISO14443-4A link\nfailed.\nTap again",
                90,
                20,
                AlignCenter,
                AlignTop);
        } else {
            popup_set_text(
                app->popup,
                "Card is not\nMIFARE Plus SL3\nor GetVersion failed",
                90,
                20,
                AlignCenter,
                AlignTop);
        }
        return true;
    }

    return false;
}

void mfp_scene_read_on_exit(void* ctx) {
    MfpApp* app = ctx;
    notification_message(app->notifications, &sequence_blink_stop);
    read_stop_poller(app);
    popup_reset(app->popup);
}
