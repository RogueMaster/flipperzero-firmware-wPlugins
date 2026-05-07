#include "../../include/nfc_tools_i.h"
#include "../../helpers/md5/nfc_tools_md5.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller.h>
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>
#include <notification/notification_messages.h>

// ── NTAG21x page addresses ───────────────────────────────────────────────────
//
//   NTAG213: CONFIG=0x29, PASSWORD=0x2B
//   NTAG215: CONFIG=0x83, PASSWORD=0x85
//   NTAG216: CONFIG=0xE3, PASSWORD=0xE5
//
// Removal sequence (source: iOS code):
//   1. PWD_AUTH(0x1B) with the current password (MD5[0..3] or FF FF FF FF)
//   2. Write CONFIG  = { 0x04, 0x00, 0x00, 0xFF }  ← AUTH0=0xFF → no protection
//   3. Write PASSWORD = { 0xFF, 0xFF, 0xFF, 0xFF }  ← restore default password

static bool ntag21x_get_pages(
    MfUltralightType type,
    uint8_t*         cfg_page,
    uint8_t*         pwd_page) {
    switch(type) {
    case MfUltralightTypeNTAG213:
        *cfg_page = 0x29;
        *pwd_page = 0x2B;
        return true;
    case MfUltralightTypeNTAG215:
        *cfg_page = 0x83;
        *pwd_page = 0x85;
        return true;
    case MfUltralightTypeNTAG216:
        *cfg_page = 0xE3;
        *pwd_page = 0xE5;
        return true;
    default:
        return false;
    }
}

// ── Write callback context ────────────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    FuriEventFlag* done;
    bool           success;
    uint8_t        auth_bytes[4]; // auth password: MD5[0..3] or FF FF FF FF
    char           err[80];
} RemovePasswordWriteCtx;

// ── nfc_poller_start_ex callback ─────────────────────────────────────────────
// Performs in a single RF session:
//   1. GET_VERSION → type + page addresses
//   2. PWD_AUTH with the current password
//   3. CONFIG  ← { 0x04, 0x00, 0x00, 0xFF }
//   4. PASSWORD ← { 0xFF, 0xFF, 0xFF, 0xFF }

static NfcCommand nfc_tools_remove_password_write_cb(NfcGenericEventEx event, void* context) {
    RemovePasswordWriteCtx* ctx    = context;
    MfUltralightPoller*     mfu    = (MfUltralightPoller*)event.poller;
    Iso14443_3aPollerEvent* iso_ev = (Iso14443_3aPollerEvent*)event.parent_event_data;

    if(iso_ev->type != Iso14443_3aPollerEventTypeReady) {
        strlcpy(ctx->err, "Tag contact error", sizeof(ctx->err));
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 1: GET_VERSION → exact type ─────────────────────────────────────
    MfUltralightVersion version = {};
    MfUltralightType    type    = MfUltralightTypeOrigin;
    if(mf_ultralight_poller_read_version(mfu, &version) == MfUltralightErrorNone) {
        type = mf_ultralight_get_type_by_version(&version);
    }

    uint8_t cfg_page = 0, pwd_page = 0;
    if(!ntag21x_get_pages(type, &cfg_page, &pwd_page)) {
        strlcpy(ctx->err, "Incompatible tag\nOnly NTAG213/215/216", sizeof(ctx->err));
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 2: PWD_AUTH with the current password ───────────────────────────
    MfUltralightPollerAuthContext auth = {};
    auth.password.data[0] = ctx->auth_bytes[0];
    auth.password.data[1] = ctx->auth_bytes[1];
    auth.password.data[2] = ctx->auth_bytes[2];
    auth.password.data[3] = ctx->auth_bytes[3];
    auth.skip_auth        = false;

    MfUltralightError auth_err = mf_ultralight_poller_auth_pwd(mfu, &auth);
    if(auth_err != MfUltralightErrorNone) {
        snprintf(
            ctx->err,
            sizeof(ctx->err),
            "Auth failed (err:%d)\nWrong password?",
            (int)auth_err);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 3: CONFIG ← { 0x04, 0x00, 0x00, 0xFF } ─────────────────────────
    // AUTH0=0xFF → no page requires authentication
    const MfUltralightPage cfg_data = {.data = {0x04, 0x00, 0x00, 0xFF}};
    MfUltralightError err = mf_ultralight_poller_write_page(mfu, cfg_page, &cfg_data);
    if(err != MfUltralightErrorNone) {
        snprintf(ctx->err, sizeof(ctx->err),
            "Config error\npage 0x%02X (err:%d)", cfg_page, (int)err);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    // ── Step 4: PASSWORD ← { 0xFF, 0xFF, 0xFF, 0xFF } ───────────────────────
    const MfUltralightPage pwd_data = {.data = {0xFF, 0xFF, 0xFF, 0xFF}};
    err = mf_ultralight_poller_write_page(mfu, pwd_page, &pwd_data);
    if(err != MfUltralightErrorNone) {
        snprintf(ctx->err, sizeof(ctx->err),
            "Password error\npage 0x%02X (err:%d)", pwd_page, (int)err);
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    ctx->success = true;
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback NfcScanner ──────────────────────────────────────────────────────

static void nfc_tools_rmpwd_scan_callback(NfcScannerEvent event, void* context) {
    NfcToolsApp* app = context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        app->detected_protocol = event.data.protocols[0];
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_DETECTED);
    }
}

// ── Worker ───────────────────────────────────────────────────────────────────

static int32_t nfc_tools_remove_password_worker(void* context) {
    NfcToolsApp* app = context;

    // ── Phase 1: detect protocol ──────────────────────────────────────────────
    NfcScanner* scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(scanner, nfc_tools_rmpwd_scan_callback, app);

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

    // ── Phase 2: verify it is MF Ultralight ──────────────────────────────────
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

    // ── Phase 3: compute authentication bytes ─────────────────────────────────
    // Empty field → default password { FF FF FF FF }
    // Non-empty field → MD5(text)[0..3]
    RemovePasswordWriteCtx write_ctx = {};
    write_ctx.app  = app;
    write_ctx.done = furi_event_flag_alloc();

    if(app->ndef_buf1[0] == '\0') {
        // No password entered → use the default password
        write_ctx.auth_bytes[0] = 0xFF;
        write_ctx.auth_bytes[1] = 0xFF;
        write_ctx.auth_bytes[2] = 0xFF;
        write_ctx.auth_bytes[3] = 0xFF;
    } else {
        uint8_t md5[16];
        nfc_tools_md5((const uint8_t*)app->ndef_buf1, strlen(app->ndef_buf1), md5);
        write_ctx.auth_bytes[0] = md5[0];
        write_ctx.auth_bytes[1] = md5[1];
        write_ctx.auth_bytes[2] = md5[2];
        write_ctx.auth_bytes[3] = md5[3];
    }

    // ── Phase 4: auth + write in a single RF session ─────────────────────────
    NfcPoller* poller = nfc_poller_alloc(app->nfc, NfcProtocolMfUltralight);
    nfc_poller_start_ex(poller, nfc_tools_remove_password_write_cb, &write_ctx);

    furi_event_flag_wait(write_ctx.done, 1u, FuriFlagWaitAny, 8000);

    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_event_flag_free(write_ctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(write_ctx.success) {
        notification_message(app->notifications, &sequence_success);
        furi_string_set(
            app->info_str,
            NTS_STATUS_PROTECTION_REMOVED_FULL);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
    } else {
        notification_message(app->notifications, &sequence_error);
        furi_string_set(app->info_str, write_ctx.err);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    }

    return 0;
}

// ── Stop helper ──────────────────────────────────────────────────────────────

static void nfc_tools_remove_password_stop_worker(NfcToolsApp* app) {
    if(app->worker_thread) {
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_STOP);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        furi_event_flag_free(app->worker_flags);
        app->worker_thread = NULL;
        app->worker_flags  = NULL;
    }
}

// ── Scene ────────────────────────────────────────────────────────────────────

void nfc_tools_scene_remove_password_write_on_enter(void* context) {
    NfcToolsApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, NTS_POPUP_REMOVE_PASSWORD, 64, 10, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        NTS_POPUP_APPROACH_NTAG,
        64, 35, AlignCenter, AlignCenter);

    furi_string_reset(app->info_str);

    app->worker_flags = furi_event_flag_alloc();
    app->worker_thread = furi_thread_alloc_ex(
        "NfcToolsRmPwd", 2 * 1024, nfc_tools_remove_password_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
}

bool nfc_tools_scene_remove_password_write_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventWriteSuccess) {
            popup_set_header(
                app->popup, NTS_STATUS_PROTECTION_REMOVED, 64, 10, AlignCenter, AlignCenter);
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

void nfc_tools_scene_remove_password_write_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_tools_remove_password_stop_worker(app);
    popup_reset(app->popup);
}
