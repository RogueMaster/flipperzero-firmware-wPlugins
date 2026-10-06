#include "magic_check.h"
#include "../nfc/card_info.h"
#include "../nfc/card_reader.h"
#include <furi.h>
#include <notification/notification_messages.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <string.h>

static void mtools_scanner_callback(NfcScannerEvent event, void* context) {
    MToolsApp* app = context;
    if(event.type != NfcScannerEventTypeDetected || app->magic_scan_found) return;
    MToolsCardProtocols protocols = mtools_card_protocols(&event);
    uint32_t result = protocols.iso15693 ? MTOOLS_EVENT_ISO :
                      protocols.iso14443a ? MTOOLS_EVENT_MFC : MTOOLS_EVENT_OTHER;
    if(result == MTOOLS_EVENT_OTHER) return;
    app->magic_type2 = protocols.type2;
    app->magic_scan_found = true;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

void mtools_magic_check_stop(MToolsApp* app) {
    furi_timer_stop(app->magic_anim_timer);
    mtools_card_reader_stop(
        app->scanner, &app->magic_scanning, &app->magic_poller, app->notifications);
}

void mtools_magic_check_start(MToolsApp* app) {
    mtools_magic_check_stop(app);
    app->scan_status = 0;
    app->magic_uid_len = 0;
    app->magic_iso_status = 2;
    app->magic_iso_blocks = 0;
    app->magic_detecting = false;
    app->magic_result_ready = false;
    app->magic_read_handled = false;
    app->magic_finish_queued = false;
    app->magic_type2 = false;
    app->magic_scan_found = false;
    mtools_card_scan_start(
        app->scanner, &app->magic_scanning, app->notifications, mtools_scanner_callback, app);
    app->magic_anim_phase = 0;
    furi_timer_start(app->magic_anim_timer, furi_ms_to_ticks(180));
    view_commit_model(app->main_view, true);
}

static NfcCommand mtools_magic_read_callback(NfcGenericEvent event, void* context) {
    MToolsApp* app = context;
    const uint8_t* uid = NULL;
    size_t uid_len = 0;
    if(event.protocol == NfcProtocolIso14443_3a) {
        Iso14443_3aPollerEvent* poller_event = event.event_data;
        if(poller_event->type != Iso14443_3aPollerEventTypeReady) return NfcCommandContinue;
        const Iso14443_3aData* card = nfc_poller_get_data(app->magic_poller);
        uid = iso14443_3a_get_uid(card, &uid_len);
        app->magic_sak = iso14443_3a_get_sak(card);
        iso14443_3a_get_atqa(card, app->magic_atqa);
        /* Some Gen3 Classic cards also answer the Ultralight scanner probe.
         * The selected card's Classic SAK takes precedence over that hint. */
        if(app->magic_sak == 0x08 || app->magic_sak == 0x18)
            app->magic_type2 = false;
    } else if(event.protocol == NfcProtocolIso15693_3) {
        Iso15693_3PollerEvent* poller_event = event.event_data;
        if(poller_event->type != Iso15693_3PollerEventTypeReady) return NfcCommandContinue;
        const Iso15693_3Data* card = nfc_poller_get_data(app->magic_poller);
        uid = iso15693_3_get_uid(card, &uid_len);
        if(!uid || uid_len != 8) return NfcCommandContinue;
        memcpy(app->magic_uid, uid, uid_len);
        uid = app->magic_uid;
        app->magic_iso_blocks = card->system_info.block_count;
        app->magic_iso_block_size = card->system_info.block_size;
        app->magic_iso_ic_ref = card->system_info.ic_ref;
        app->magic_uid_len = uid_len;
        app->scan_status = 2;
        app->magic_detecting = true;
        app->magic_result_ready = false;
        app->magic_read_handled = false;
        app->magic_anim_start_tick = furi_get_tick();
        app->magic_anim_phase = 0;
        view_commit_model(app->main_view, true);
        MagicGenType gen =
            mtools_detect_iso15693_ready(event.instance, &card->system_info, uid, uid_len);
        app->magic_iso_status = gen == MagicGenIso15693Gen1 ? 4 :
                                gen == MagicGenIso15693Gen3 ? 5 :
                                gen == MagicGenIso15693Gen2 ? 3 : 2;
        app->magic_pending_status = app->magic_iso_status;
        app->magic_result_ready = true;
    }
    if(!uid || uid_len == 0 || uid_len > sizeof(app->magic_uid)) return NfcCommandContinue;
    if(uid != app->magic_uid) memcpy(app->magic_uid, uid, uid_len);
    app->magic_uid_len = uid_len;
    view_dispatcher_send_custom_event(app->view_dispatcher, MTOOLS_EVENT_MAGIC_READ);
    return NfcCommandStop;
}

static void mtools_magic_finish_result(MToolsApp* app) {
    furi_timer_stop(app->magic_anim_timer);
    app->scan_status = app->magic_pending_status;
    app->magic_detecting = false;
    app->magic_finish_queued = false;
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_success);
    view_commit_model(app->main_view, true);
}

bool mtools_magic_check_event(MToolsApp* app, uint32_t event) {
    if(event == MTOOLS_EVENT_MAGIC_FINISH) {
        if(app->magic_detecting && app->magic_result_ready && app->magic_read_handled &&
           furi_get_tick() - app->magic_anim_start_tick >=
               furi_ms_to_ticks(MTOOLS_MAGIC_MIN_ANIM_MS))
            mtools_magic_finish_result(app);
        else
            app->magic_finish_queued = false;
        return true;
    }
    if(event == MTOOLS_EVENT_MAGIC_TICK) {
        if(app->scan_status == 0 || app->scan_status == 6) {
            app->magic_anim_phase++;
            view_commit_model(app->main_view, true);
        }
        return true;
    }
    if(event == MTOOLS_EVENT_RESCAN) {
        mtools_magic_check_start(app);
        return true;
    }
    if(event < MTOOLS_EVENT_MFC || event > MTOOLS_EVENT_MAGIC_READ) return false;
    if(event == MTOOLS_EVENT_MAGIC_READ) {
        mtools_magic_check_stop(app);
        if(app->magic_protocol == 1) {
            /* ISO15693 generation was checked while its original poller was ready. */
            app->magic_read_handled = true;
        } else {
            app->scan_status = 1;
            app->magic_detecting = true;
            app->magic_result_ready = false;
            app->magic_read_handled = false;
            app->magic_anim_start_tick = furi_get_tick();
            app->magic_anim_phase = 0;
            view_commit_model(app->main_view, true);
            furi_timer_start(app->magic_anim_timer, furi_ms_to_ticks(90));
            if(!app->magic_type2 && (app->magic_sak == 0x08 || app->magic_sak == 0x18)) {
                notification_message(app->notifications, &sequence_blink_start_blue);
                if(mtools_detect_magic_tag(app->nfc, MagicGenMfcGen4))
                    app->magic_pending_status = 10;
                else if(mtools_detect_magic_tag(app->nfc, MagicGenMfcGdm))
                    app->magic_pending_status = 11;
                else if(mtools_detect_magic_tag(app->nfc, MagicGenMfcGen3))
                    app->magic_pending_status = 9;
                else if(mtools_detect_magic_tag(app->nfc, MagicGenMfcGen1a))
                    app->magic_pending_status = 7;
                else if(mtools_detect_magic_tag(app->nfc, MagicGenMfcGen2))
                    app->magic_pending_status = 8;
                else
                    app->magic_pending_status = 1;
            } else {
                app->magic_pending_status = 1;
            }
            app->magic_result_ready = true;
            app->magic_read_handled = true;
        }
        if(furi_get_tick() - app->magic_anim_start_tick >=
           furi_ms_to_ticks(MTOOLS_MAGIC_MIN_ANIM_MS))
            mtools_magic_finish_result(app);
        else if(app->magic_protocol == 1)
            furi_timer_start(app->magic_anim_timer, furi_ms_to_ticks(90));
        view_commit_model(app->main_view, true);
        return true;
    }
    mtools_magic_check_stop(app);
    app->magic_protocol = event == MTOOLS_EVENT_ISO ? 1 : 0;
    app->scan_status = 6;
    app->magic_poller = nfc_poller_alloc(
        app->nfc, app->magic_protocol == 1 ? NfcProtocolIso15693_3 : NfcProtocolIso14443_3a);
    nfc_poller_start(app->magic_poller, mtools_magic_read_callback, app);
    furi_timer_start(app->magic_anim_timer, furi_ms_to_ticks(180));
    view_commit_model(app->main_view, true);
    return true;
}

void mtools_magic_check_timer_callback(void* context) {
    MToolsApp* app = context;
    if(app->magic_detecting) {
        app->magic_anim_phase++;
        view_commit_model(app->main_view, true);
        if(app->magic_result_ready && app->magic_read_handled && !app->magic_finish_queued &&
           furi_get_tick() - app->magic_anim_start_tick >=
               furi_ms_to_ticks(MTOOLS_MAGIC_MIN_ANIM_MS)) {
            app->magic_finish_queued = true;
            view_dispatcher_send_custom_event(app->view_dispatcher, MTOOLS_EVENT_MAGIC_FINISH);
        }
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, MTOOLS_EVENT_MAGIC_TICK);
    }
}
