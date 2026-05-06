#include "../../include/nfc_tools_i.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>
#include <notification/notification_messages.h>

// ── Permanent lock of an NTAG21x tag ─────────────────────────────────────────
//
// Sequence:
//   1. GET_VERSION → exact type (NTAG213 / 215 / 216)
//   2. Read page 2 → retrieve bytes 0-1 (UID/internal, read-only)
//   3. Write page 2 = { b0, b1, 0xFF, 0xFF }
//      → bytes 2-3: static lock bits all set to 1 = pages 3-15 locked
//   4. Write dynamic lock page → pages 16 to end of user memory
//      NTAG213: page 0x28, { 0xFF, 0xFF, 0x00, 0x00 }  (covers 0x04-0x27)
//      NTAG215: page 0x82, { 0xFF, 0xFF, 0xFF, 0x00 }  (covers 0x04-0x81)
//      NTAG216: page 0xE2, { 0xFF, 0xFF, 0xFF, 0x00 }  (covers 0x04-0xE1)
//
// ⚠️  This operation is IRREVERSIBLE — once lock bits are set to 1 they cannot
//     be cleared back to 0.

// ── Contexte du callback ──────────────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    FuriEventFlag* done;
    bool           success;
    char           err[80];
} LockTagCtx;

// ── Callback nfc_poller_start_ex ──────────────────────────────────────────────

static NfcCommand nfc_tools_lock_tag_cb(NfcGenericEventEx event, void* context) {
    LockTagCtx*             ctx    = context;
    MfUltralightPoller*     mfu    = (MfUltralightPoller*)event.poller;
    Iso14443_3aPollerEvent* iso_ev = (Iso14443_3aPollerEvent*)event.parent_event_data;

    if(iso_ev->type != Iso14443_3aPollerEventTypeReady) {
        strlcpy(ctx->err, "Tag contact error", sizeof(ctx->err));
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 1: identify type via GET_VERSION ────────────────────────────────
    MfUltralightVersion version = {};
    MfUltralightType    type    = MfUltralightTypeOrigin;
    if(mf_ultralight_poller_read_version(mfu, &version) == MfUltralightErrorNone) {
        type = mf_ultralight_get_type_by_version(&version);
    }

    if(type != MfUltralightTypeNTAG213 &&
       type != MfUltralightTypeNTAG215 &&
       type != MfUltralightTypeNTAG216) {
        strlcpy(ctx->err, "Incompatible tag\nOnly NTAG213/215/216", sizeof(ctx->err));
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 2: read page 2 to preserve bytes 0-1 (UID/internal) ────────────
    MfUltralightPageReadCommandData read_data = {};
    MfUltralightError err = mf_ultralight_poller_read_page(mfu, 2, &read_data);
    if(err != MfUltralightErrorNone) {
        snprintf(ctx->err, sizeof(ctx->err), "Read error\npage 2 (err:%d)", (int)err);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    uint8_t b0 = read_data.page[0].data[0]; // UID / internal (read-only on the chip)
    uint8_t b1 = read_data.page[0].data[1];

    // ── Step 3: static lock bits → 0xFF 0xFF ─────────────────────────────────
    const MfUltralightPage static_lock = {.data = {b0, b1, 0xFF, 0xFF}};
    err = mf_ultralight_poller_write_page(mfu, 2, &static_lock);
    if(err != MfUltralightErrorNone) {
        snprintf(
            ctx->err, sizeof(ctx->err),
            "Lock failed\n(err:%d)\nRemove pwd first?", (int)err);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 4: dynamic lock bits (all NTAG21x models) ───────────────────────
    //
    // NTAG213 → page 0x28 : {0xFF, 0xFF, 0x00, 0x00}
    //   couvre pages 16-39 (bytes 0-1 = lock bits, bytes 2-3 = RFUI)
    // NTAG215 → page 0x82 : {0xFF, 0xFF, 0xFF, 0x00}
    //   couvre pages 16-129 (bytes 0-2 = lock bits, byte 3 = RFUI)
    // NTAG216 → page 0xE2 : {0xFF, 0xFF, 0xFF, 0x00}
    //   couvre pages 16-225 (bytes 0-2 = lock bits, byte 3 = RFUI)
    {
        uint8_t dyn_page;
        MfUltralightPage dyn_lock;

        if(type == MfUltralightTypeNTAG213) {
            dyn_page = 0x28;
            dyn_lock = (MfUltralightPage){.data = {0xFF, 0xFF, 0x00, 0x00}};
        } else if(type == MfUltralightTypeNTAG215) {
            dyn_page = 0x82;
            dyn_lock = (MfUltralightPage){.data = {0xFF, 0xFF, 0xFF, 0x00}};
        } else { // NTAG216
            dyn_page = 0xE2;
            dyn_lock = (MfUltralightPage){.data = {0xFF, 0xFF, 0xFF, 0x00}};
        }

        err = mf_ultralight_poller_write_page(mfu, dyn_page, &dyn_lock);
        if(err != MfUltralightErrorNone) {
            snprintf(
                ctx->err, sizeof(ctx->err),
                "Dyn lock failed\npage 0x%02X (err:%d)", dyn_page, (int)err);
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }
    }

    ctx->success = true;
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback NfcScanner ───────────────────────────────────────────────────────

static void nfc_tools_lock_scan_callback(NfcScannerEvent event, void* context) {
    NfcToolsApp* app = context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        app->detected_protocol = event.data.protocols[0];
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_DETECTED);
    }
}

// ── Worker ────────────────────────────────────────────────────────────────────

static int32_t nfc_tools_lock_tag_worker(void* context) {
    NfcToolsApp* app = context;

    // Phase 1: detect protocol
    NfcScanner* scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(scanner, nfc_tools_lock_scan_callback, app);

    uint32_t flags = furi_event_flag_wait(
        app->worker_flags,
        NFC_TOOLS_WORKER_FLAG_DETECTED | NFC_TOOLS_WORKER_FLAG_STOP,
        FuriFlagWaitAny,
        15000);

    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    if(flags & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(flags == (uint32_t)FuriFlagErrorTimeout) {
        notification_message(app->notifications, &sequence_error);
        furi_string_set(app->info_str, NTS_ERR_NO_TAG);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    // Phase 2: verify it is MF Ultralight
    if(app->detected_protocol != NfcProtocolMfUltralight) {
        notification_message(app->notifications, &sequence_error);
        furi_string_printf(
            app->info_str,
            "Incompatible tag\n(%s)\nOnly NTAG21x",
            nfc_device_get_protocol_name(app->detected_protocol));
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    // Phase 3: lock in a single RF session
    LockTagCtx lock_ctx = {};
    lock_ctx.app  = app;
    lock_ctx.done = furi_event_flag_alloc();

    NfcPoller* poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start_ex(poller, nfc_tools_lock_tag_cb, &lock_ctx);

    furi_event_flag_wait(lock_ctx.done, 1u, FuriFlagWaitAny, 8000);

    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_event_flag_free(lock_ctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(lock_ctx.success) {
        notification_message(app->notifications, &sequence_success);
        furi_string_set(
            app->info_str,
            NTS_STATUS_TAG_LOCKED_INFO);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
    } else {
        notification_message(app->notifications, &sequence_error);
        furi_string_set(app->info_str, lock_ctx.err);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    }

    return 0;
}

// ── Stop helper ───────────────────────────────────────────────────────────────

static void nfc_tools_lock_tag_stop_worker(NfcToolsApp* app) {
    if(app->worker_thread) {
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_STOP);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        furi_event_flag_free(app->worker_flags);
        app->worker_thread = NULL;
        app->worker_flags  = NULL;
    }
}

// ── Scene ─────────────────────────────────────────────────────────────────────

void nfc_tools_scene_lock_tag_on_enter(void* context) {
    NfcToolsApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, NTS_POPUP_LOCK_TAG, 64, 10, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        NTS_POPUP_LOCK_WARNING,
        64, 32, AlignCenter, AlignCenter);

    furi_string_reset(app->info_str);

    app->worker_flags = furi_event_flag_alloc();
    app->worker_thread =
        furi_thread_alloc_ex("NfcToolsLock", 2 * 1024, nfc_tools_lock_tag_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
}

bool nfc_tools_scene_lock_tag_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventWriteSuccess) {
            popup_set_header(app->popup, NTS_STATUS_TAG_LOCKED, 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                furi_string_get_cstr(app->info_str),
                64, 32, AlignCenter, AlignCenter);
            consumed = true;
        } else if(event.event == NfcToolsEventWriteFail) {
            popup_set_header(app->popup, NTS_ERR_FAILED, 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                furi_string_get_cstr(app->info_str),
                64, 32, AlignCenter, AlignCenter);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_tools_scene_lock_tag_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_tools_lock_tag_stop_worker(app);
    popup_reset(app->popup);
}
