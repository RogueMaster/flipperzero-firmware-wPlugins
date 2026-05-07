#include "../../include/nfc_tools_i.h"
#include "../../include/nfc_tools_ndef.h"
#include "../../include/nfc_tools_mfc.h"
#include "../../include/nfc_tools_desfire.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/felica/felica.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>
#include <nfc/protocols/iso15693_3/iso15693_3_poller.h>
#include <nfc/protocols/mf_desfire/mf_desfire_poller.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <lib/toolbox/simple_array.h>

// ── Callback + contexte ISO 15693 ───────────────────────────────────────────

typedef struct {
    NfcToolsApp* app;
    NfcPoller*   poller;
    FuriEventFlag* done;
    bool           success;
} Iso15693ReadCtx;

static NfcCommand nfc_tools_iso15693_read_cb(NfcGenericEvent event, void* context) {
    Iso15693ReadCtx* ctx = context;
    const Iso15693_3PollerEvent* ev = event.event_data;

    if(ev->type == Iso15693_3PollerEventTypeReady) {
        NfcToolsApp* app = ctx->app;
        const Iso15693_3Data* iso =
            (const Iso15693_3Data*)nfc_poller_get_data(ctx->poller);

        // UID (8 bytes, most significant byte first in the SDK array)
        app->uid_len = ISO15693_3_UID_SIZE;
        memcpy(app->uid, iso->uid, ISO15693_3_UID_SIZE);

        // Memory info
        app->iso15693_block_count = iso15693_3_get_block_count(iso);
        app->iso15693_block_size  = iso15693_3_get_block_size(iso);

        // IC Reference (enables precise SLI model identification)
        app->iso15693_ic_ref = iso->system_info.ic_ref;

        // NDEF ICODE read: block 0 = CC (factory, often locked).
        // The NDEF TLV starts at block 1 (NFC Forum T5T).
        if(app->iso15693_block_count > 1 && app->iso15693_block_size > 0) {
            uint16_t bc  = app->iso15693_block_count;
            uint8_t  bs  = app->iso15693_block_size;
            uint16_t start = 1; // skip the CC at block 0
            size_t   len = (size_t)(bc - start) * bs;
            uint8_t* buf = malloc(len);
            if(buf) {
                for(uint16_t b = start; b < bc; b++) {
                    const uint8_t* bd = iso15693_3_get_block_data(iso, (uint8_t)b);
                    if(bd) memcpy(buf + (b - start) * bs, bd, bs);
                }
                nfc_tools_ndef_parse_type2_tag(buf, len, app->ndef_str);
                nfc_tools_ndef_parse_type2_tag_structured(app, buf, len);
                free(buf);
            }
        }
        ctx->success = true;
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback + contexte DESFire ─────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    bool           success;
} DesfireReadCtx;

static NfcCommand nfc_tools_desfire_read_cb(NfcGenericEvent event, void* context) {
    DesfireReadCtx* ctx = context;
    const MfDesfirePollerEvent* ev = event.event_data;

    if(ev->type == MfDesfirePollerEventTypeReadSuccess) {
        NfcToolsApp* app = ctx->app;
        const MfDesfireData* d = (const MfDesfireData*)nfc_poller_get_data(ctx->poller);

        // UID
        size_t uid_len = 0;
        const uint8_t* uid = mf_desfire_get_uid(d, &uid_len);
        app->uid_len = uid_len < sizeof(app->uid) ? uid_len : sizeof(app->uid);
        memcpy(app->uid, uid, app->uid_len);

        // Type (hw_major)
        app->desfire_hw_major = d->version.hw_major;

        // Free memory
        app->desfire_has_free_memory = d->free_memory.is_present;
        app->desfire_free_memory     = d->free_memory.bytes_free;

        // Nombre d'applications
        app->desfire_app_count = simple_array_get_count(d->application_ids);

        app->sak     = 0;
        app->atqa[0] = app->atqa[1] = 0;

        ctx->success = true;
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback + contexte FeliCa ──────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    bool           success;
} FelicaReadCtx;

static NfcCommand nfc_tools_felica_read_cb(NfcGenericEvent event, void* context) {
    FelicaReadCtx*     ctx = context;
    FelicaPollerEvent* ev  = event.event_data;

    if(ev->type == FelicaPollerEventTypeRequestAuthContext) {
        ev->data->auth_context->skip_auth = true;
        return NfcCommandContinue;
    }

    if(ev->type == FelicaPollerEventTypeReady ||
       ev->type == FelicaPollerEventTypeIncomplete) {
        NfcToolsApp*      app    = ctx->app;
        const FelicaData* felica = (const FelicaData*)nfc_poller_get_data(ctx->poller);

        // IDm → UID
        size_t uid_len;
        const uint8_t* uid = felica_get_uid(felica, &uid_len);
        app->uid_len = uid_len < sizeof(app->uid) ? uid_len : sizeof(app->uid);
        memcpy(app->uid, uid, app->uid_len);

        // PMm
        memcpy(app->felica_pmm, felica->pmm.data, FELICA_PMM_SIZE);

        // IC name (e.g. "RC-S960", "RC-S965", ...)
        FuriString* ic_str = furi_string_alloc();
        felica_get_ic_name(felica, ic_str);
        strlcpy(app->felica_ic_name, furi_string_get_cstr(ic_str), sizeof(app->felica_ic_name));
        furi_string_free(ic_str);

        // Blocs
        app->felica_blocks_read    = felica->blocks_read;
        app->felica_blocks_total   = felica->blocks_total;
        app->felica_workflow_type  = felica->workflow_type;
        app->sak                   = 0;
        app->atqa[0] = app->atqa[1] = 0;

        ctx->success = true;
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback NfcScanner ─────────────────────────────────────────────────────

static void nfc_tools_scan_callback(NfcScannerEvent event, void* context) {
    NfcToolsApp* app = context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        app->detected_protocol = event.data.protocols[0];
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_DETECTED);
    }
}

// ── Worker ──────────────────────────────────────────────────────────────────

static int32_t nfc_tools_read_worker(void* context) {
    NfcToolsApp* app = context;

    // Phase 1: detect NFC protocol via scanner
    NfcScanner* scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(scanner, nfc_tools_scan_callback, app);

    uint32_t flags = furi_event_flag_wait(
        app->worker_flags,
        NFC_TOOLS_WORKER_FLAG_DETECTED | NFC_TOOLS_WORKER_FLAG_STOP,
        FuriFlagWaitAny,
        10000);

    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    bool stopped = (flags & NFC_TOOLS_WORKER_FLAG_STOP) != 0;
    bool timed_out = (flags == (uint32_t)FuriFlagErrorTimeout);

    if(stopped) return 0;
    if(timed_out) {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventScanTimeout);
        return 0;
    }

    // Reset ISO 15693 fields + MfUltralight version + NDEF records
    app->iso15693_block_count  = 0;
    app->iso15693_block_size   = 0;
    app->iso15693_ic_ref       = 0;
    app->mful_version_valid    = false;
    app->ndef_record_count     = 0;
    app->ndef_selected_record  = 0;
    furi_string_reset(app->ndef_str);

    bool is_iso15693 = (app->detected_protocol == NfcProtocolIso15693_3 ||
                        app->detected_protocol == NfcProtocolSlix);
    bool is_desfire  = (app->detected_protocol == NfcProtocolMfDesfire);
    bool is_felica   = (app->detected_protocol == NfcProtocolFelica);
    bool ok = false;

    if(is_iso15693) {
        // ── Phase 2b : lecture ISO 15693 via NfcPoller ──────────────────────
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        app->uid_len    = 0;
        app->sak        = 0;
        app->atqa[0]    = app->atqa[1] = 0;

        Iso15693ReadCtx iso_ctx = {
            .app     = app,
            .done    = furi_event_flag_alloc(),
            .success = false,
        };
        iso_ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
        nfc_poller_start(iso_ctx.poller, nfc_tools_iso15693_read_cb, &iso_ctx);

        furi_event_flag_wait(iso_ctx.done, 1u, FuriFlagWaitAny, 5000);

        nfc_poller_stop(iso_ctx.poller);
        nfc_poller_free(iso_ctx.poller);
        furi_event_flag_free(iso_ctx.done);
        ok = iso_ctx.success;

    } else if(is_desfire) {
        // ── Phase 2c : lecture DESFire via NfcPoller ─────────────────────────
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        app->uid_len = 0;
        app->sak     = 0;
        app->atqa[0] = app->atqa[1] = 0;

        DesfireReadCtx desfire_ctx = {
            .app     = app,
            .done    = furi_event_flag_alloc(),
            .success = false,
        };
        desfire_ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolMfDesfire);
        nfc_poller_start(desfire_ctx.poller, nfc_tools_desfire_read_cb, &desfire_ctx);

        furi_event_flag_wait(desfire_ctx.done, 1u, FuriFlagWaitAny, 5000);

        nfc_poller_stop(desfire_ctx.poller);
        nfc_poller_free(desfire_ctx.poller);
        furi_event_flag_free(desfire_ctx.done);
        ok = desfire_ctx.success;

        // Phase 2c-bis: NDEF read via T4T APDUs (ISO14443-4A poller)
        // Non-fatal: a failure leaves ndef_record_count at 0.
        if(ok) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;
            nfc_tools_desfire_read_ndef(app);
        }

    } else if(is_felica) {
        // ── Phase 2d : lecture FeliCa via NfcPoller ─────────────────────────
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        app->uid_len = 0;
        app->sak     = 0;
        app->atqa[0] = app->atqa[1] = 0;
        furi_string_reset(app->ndef_str);

        FelicaReadCtx felica_ctx = {
            .app     = app,
            .done    = furi_event_flag_alloc(),
            .success = false,
        };
        felica_ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolFelica);
        nfc_poller_start(felica_ctx.poller, nfc_tools_felica_read_cb, &felica_ctx);

        furi_event_flag_wait(felica_ctx.done, 1u, FuriFlagWaitAny, 8000);

        nfc_poller_stop(felica_ctx.poller);
        nfc_poller_free(felica_ctx.poller);
        furi_event_flag_free(felica_ctx.done);
        ok = felica_ctx.success;

    } else {
        // ── Phase 2 : lecture UID/SAK/ATQA via poller ISO 14443-3A ──────────
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        NfcPoller* iso_poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
        ok = nfc_poller_detect(iso_poller);

        if(ok) {
            const Iso14443_3aData* iso =
                (const Iso14443_3aData*)nfc_poller_get_data(iso_poller);
            app->uid_len =
                iso->uid_len < sizeof(app->uid) ? iso->uid_len : sizeof(app->uid);
            memcpy(app->uid, iso->uid, app->uid_len);
            app->sak = iso->sak;
            memcpy(app->atqa, iso->atqa, sizeof(app->atqa));

            if(app->detected_protocol == NfcProtocolMfClassic) {
                if((app->sak & 0x18) == 0x18)
                    app->mfc_type = MfClassicType4k;
                else if((app->sak & 0x09) == 0x09)
                    app->mfc_type = MfClassicTypeMini;
                else
                    app->mfc_type = MfClassicType1k;
            }
        } else {
            app->uid_len    = 0;
            app->sak        = 0;
            app->atqa[0]    = app->atqa[1] = 0;
        }
        nfc_poller_free(iso_poller);

        // ── Phase 3 : MfUltralight — type exact + lecture NDEF ──────────────
        if(ok && app->detected_protocol == NfcProtocolMfUltralight) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

            MfUltralightVersion version = {};
            if(mf_ultralight_poller_sync_read_version(app->nfc, &version) ==
               MfUltralightErrorNone) {
                app->mful_type          = mf_ultralight_get_type_by_version(&version);
                app->mful_version       = version;
                app->mful_version_valid = true;
            } else {
                app->mful_type          = MfUltralightTypeOrigin;
                app->mful_version_valid = false;
            }

            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

            MfUltralightData* mful = mf_ultralight_alloc();
            if(mf_ultralight_poller_sync_read_card(app->nfc, mful, NULL) ==
                   MfUltralightErrorNone &&
               mful->pages_read > 4) {
                size_t user_pages = mful->pages_read - 4;
                size_t data_len   = user_pages * MF_ULTRALIGHT_PAGE_SIZE;
                uint8_t* data     = malloc(data_len);
                for(size_t p = 0; p < user_pages; p++) {
                    memcpy(data + p * MF_ULTRALIGHT_PAGE_SIZE,
                           mful->page[4 + p].data,
                           MF_ULTRALIGHT_PAGE_SIZE);
                }
                nfc_tools_ndef_parse_type2_tag(data, data_len, app->ndef_str);
                nfc_tools_ndef_parse_type2_tag_structured(app, data, data_len);
                free(data);
            }
            mf_ultralight_free(mful);
        }

        // ── Phase 4 : Mifare Classic — lecture MAD + NDEF ───────────────────
        if(ok && app->detected_protocol == NfcProtocolMfClassic) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;
            nfc_tools_mfc_read_ndef(app);
        }
    }

    // Final STOP check before sending the event
    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(ok) {
        notification_message(app->notifications, &sequence_success);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventScanSuccess);
    } else {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventScanTimeout);
    }

    return 0;
}

// ── Stop helper ─────────────────────────────────────────────────────────────

static void nfc_tools_read_tag_stop_worker(NfcToolsApp* app) {
    if(app->worker_thread) {
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_STOP);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        furi_event_flag_free(app->worker_flags);
        app->worker_thread = NULL;
        app->worker_flags = NULL;
    }
}

// ── Scene ───────────────────────────────────────────────────────────────────

void nfc_tools_scene_read_tag_on_enter(void* context) {
    NfcToolsApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, NTS_POPUP_APPROACH_TAG, 64, 10, AlignCenter, AlignCenter);
    popup_set_text(app->popup, NTS_POPUP_BACK_TO_CANCEL, 64, 40, AlignCenter, AlignCenter);

    app->worker_flags = furi_event_flag_alloc();
    app->worker_thread =
        furi_thread_alloc_ex("NfcToolsRead", 4 * 1024, nfc_tools_read_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
}

bool nfc_tools_scene_read_tag_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventScanSuccess) {
            scene_manager_next_scene(app->scene_manager, app->scan_destination);
            consumed = true;
        } else if(event.event == NfcToolsEventScanTimeout) {
            popup_set_header(
                app->popup, NTS_ERR_NO_TAG, 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, NTS_POPUP_BACK_TO_RETURN, 64, 40, AlignCenter, AlignCenter);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_tools_scene_read_tag_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_tools_read_tag_stop_worker(app);
    popup_reset(app->popup);
}
