#include "../../include/nfc_tools_i.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <toolbox/bit_buffer.h>
#include <notification/notification_messages.h>

// ── NFC Commands: send a raw APDU command ────────────────────────────────────
//
// Flow:
//   1. NfcScanner → detect protocol
//   2. Route to ISO 14443-4A  (DESFire, MfPlus, ISO 14443-4A)
//          or ISO 14443-3A  (MfUltralight, MfClassic, ISO 14443-3A)
//   3. Send bytes, display response in a TextBox

// ── Helper: hex decode ────────────────────────────────────────────────────────

static int hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Returns the number of decoded bytes, or -1 if the format is invalid.
static int hex_parse(const char* s, uint8_t* out, size_t max_out) {
    size_t count = 0;
    while(*s) {
        // Skip spaces
        if(*s == ' ' || *s == '\t') { s++; continue; }
        int hi = hex_nibble(*s++);
        if(hi < 0 || !*s) return -1;
        // Skip spaces between bytes
        while(*s == ' ' || *s == '\t') s++;
        int lo = hex_nibble(*s++);
        if(lo < 0) return -1;
        if(count >= max_out) return -1;
        out[count++] = (uint8_t)((hi << 4) | lo);
    }
    return (int)count;
}

// ── Shared context ────────────────────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    FuriEventFlag* done;
    const uint8_t* cmd;
    size_t         cmd_len;
    uint8_t        rsp[258]; // raw response (max APDU + 2 SW bytes)
    size_t         rsp_len;
    bool           success;
    char           err[80];
} NfcCommandsCtx;

// ── Callback ISO 14443-4A ─────────────────────────────────────────────────────

static NfcCommand nfc_commands_iso4a_cb(NfcGenericEvent event, void* context) {
    NfcCommandsCtx*         ctx = context;
    Iso14443_4aPollerEvent* ev  = event.event_data;

    if(ev->type != Iso14443_4aPollerEventTypeReady) {
        strlcpy(ctx->err, "Tag contact error", sizeof(ctx->err));
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    Iso14443_4aPoller* poller = event.instance;

    BitBuffer* tx = bit_buffer_alloc(ctx->cmd_len);
    BitBuffer* rx = bit_buffer_alloc(258);

    bit_buffer_append_bytes(tx, ctx->cmd, ctx->cmd_len);

    Iso14443_4aError err = iso14443_4a_poller_send_block(poller, tx, rx);

    if(err == Iso14443_4aErrorNone) {
        ctx->rsp_len = bit_buffer_get_size_bytes(rx);
        if(ctx->rsp_len > sizeof(ctx->rsp)) ctx->rsp_len = sizeof(ctx->rsp);
        memcpy(ctx->rsp, bit_buffer_get_data(rx), ctx->rsp_len);
        ctx->success = true;
    } else {
        snprintf(ctx->err, sizeof(ctx->err), "ISO 14443-4A error\n(code %d)", (int)err);
    }

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback ISO 14443-3A ─────────────────────────────────────────────────────

static NfcCommand nfc_commands_iso3a_cb(NfcGenericEvent event, void* context) {
    NfcCommandsCtx*         ctx = context;
    Iso14443_3aPollerEvent* ev  = event.event_data;

    if(ev->type != Iso14443_3aPollerEventTypeReady) {
        strlcpy(ctx->err, "Tag contact error", sizeof(ctx->err));
        furi_event_flag_set(ctx->done, 1u);
        return NfcCommandStop;
    }

    Iso14443_3aPoller* poller = event.instance;

    BitBuffer* tx = bit_buffer_alloc(ctx->cmd_len);
    BitBuffer* rx = bit_buffer_alloc(258);

    bit_buffer_append_bytes(tx, ctx->cmd, ctx->cmd_len);

    // FWT = 200 000 clock cycles (~ 14 ms at 13.56 MHz)
    Iso14443_3aError err =
        iso14443_3a_poller_send_standard_frame(poller, tx, rx, 200000);

    if(err == Iso14443_3aErrorNone) {
        ctx->rsp_len = bit_buffer_get_size_bytes(rx);
        if(ctx->rsp_len > sizeof(ctx->rsp)) ctx->rsp_len = sizeof(ctx->rsp);
        memcpy(ctx->rsp, bit_buffer_get_data(rx), ctx->rsp_len);
        ctx->success = true;
    } else {
        snprintf(ctx->err, sizeof(ctx->err), "ISO 14443-3A error\n(code %d)", (int)err);
    }

    bit_buffer_free(tx);
    bit_buffer_free(rx);
    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Callback NfcScanner ───────────────────────────────────────────────────────

static void nfc_commands_scan_callback(NfcScannerEvent event, void* context) {
    NfcToolsApp* app = context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        app->detected_protocol = event.data.protocols[0];
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_DETECTED);
    }
}

// ── Check whether the protocol uses ISO 14443-4A ─────────────────────────────

static bool protocol_is_iso4a(NfcProtocol p) {
    return p == NfcProtocolIso14443_4a ||
           p == NfcProtocolMfDesfire   ||
           p == NfcProtocolMfPlus;
}

// ── Format response into info_str ─────────────────────────────────────────────

static void format_result(
    NfcToolsApp*   app,
    const uint8_t* cmd,
    size_t         cmd_len,
    const uint8_t* rsp,
    size_t         rsp_len)
{
    furi_string_reset(app->info_str);

    // TX
    furi_string_cat_str(app->info_str, "> ");
    for(size_t i = 0; i < cmd_len; i++) {
        furi_string_cat_printf(app->info_str, "%02X", cmd[i]);
    }
    furi_string_cat_str(app->info_str, "\n");

    // RX
    if(rsp_len == 0) {
        furi_string_cat_str(app->info_str, "< (empty)\n");
        return;
    }

    furi_string_cat_str(app->info_str, "< ");
    for(size_t i = 0; i < rsp_len; i++) {
        furi_string_cat_printf(app->info_str, "%02X", rsp[i]);
        if(i < rsp_len - 1) furi_string_cat_str(app->info_str, " ");
    }
    furi_string_cat_str(app->info_str, "\n");

    // SW if at least 2 bytes
    if(rsp_len >= 2) {
        uint8_t sw1 = rsp[rsp_len - 2];
        uint8_t sw2 = rsp[rsp_len - 1];
        const char* label =
            (sw1 == 0x90 && sw2 == 0x00) ? "OK" :
            (sw1 == 0x61)                 ? "More" :
            (sw1 == 0x6A && sw2 == 0x82) ? "Not Found" :
            (sw1 == 0x6A && sw2 == 0x86) ? "Bad P1/P2" :
            (sw1 == 0x67 && sw2 == 0x00) ? "Wrong Len" :
            (sw1 == 0x69 && sw2 == 0x82) ? "Security" :
            (sw1 == 0x69 && sw2 == 0x85) ? "Conditions" : NULL;

        if(label)
            furi_string_cat_printf(app->info_str, "SW: %02X %02X [%s]", sw1, sw2, label);
        else
            furi_string_cat_printf(app->info_str, "SW: %02X %02X", sw1, sw2);
    }
}

// ── Worker ────────────────────────────────────────────────────────────────────

static int32_t nfc_commands_worker(void* context) {
    NfcToolsApp* app = context;

    // Hex decode the command
    static uint8_t cmd_bytes[128];
    int cmd_len = hex_parse(app->ndef_buf1, cmd_bytes, sizeof(cmd_bytes));

    if(cmd_len <= 0) {
        furi_string_set(app->info_str, NTS_ERR_INVALID_HEX);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    // Phase 1: detect protocol
    NfcScanner* scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(scanner, nfc_commands_scan_callback, app);

    uint32_t flags = furi_event_flag_wait(
        app->worker_flags,
        NFC_TOOLS_WORKER_FLAG_DETECTED | NFC_TOOLS_WORKER_FLAG_STOP,
        FuriFlagWaitAny,
        15000);

    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    if(flags & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(flags == (uint32_t)FuriFlagErrorTimeout) {
        furi_string_set(app->info_str, NTS_ERR_NO_TAG);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    // Phase 2: send the command
    NfcCommandsCtx cmd_ctx = {
        .app     = app,
        .done    = furi_event_flag_alloc(),
        .cmd     = cmd_bytes,
        .cmd_len = (size_t)cmd_len,
        .success = false,
    };

    bool is4a = protocol_is_iso4a(app->detected_protocol);
    NfcProtocol poller_proto =
        is4a ? NfcProtocolIso14443_4a : NfcProtocolIso14443_3a;

    NfcPoller* poller = nfc_poller_alloc(app->nfc, poller_proto);
    nfc_poller_start(
        poller,
        is4a ? nfc_commands_iso4a_cb : nfc_commands_iso3a_cb,
        &cmd_ctx);

    furi_event_flag_wait(cmd_ctx.done, 1u, FuriFlagWaitAny, 8000);

    nfc_poller_stop(poller);
    nfc_poller_free(poller);
    furi_event_flag_free(cmd_ctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(cmd_ctx.success) {
        notification_message(app->notifications, &sequence_success);
        format_result(app, cmd_bytes, (size_t)cmd_len, cmd_ctx.rsp, cmd_ctx.rsp_len);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
    } else {
        notification_message(app->notifications, &sequence_error);
        furi_string_set(app->info_str, cmd_ctx.err);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    }

    return 0;
}

// ── Stop helper ───────────────────────────────────────────────────────────────

static void nfc_commands_stop_worker(NfcToolsApp* app) {
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

void nfc_tools_scene_nfc_commands_run_on_enter(void* context) {
    NfcToolsApp* app = context;

    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, NTS_NFC_CMDS_WAITING);

    furi_string_reset(app->info_str);

    app->worker_flags = furi_event_flag_alloc();
    app->worker_thread =
        furi_thread_alloc_ex("NfcToolsCmds", 2 * 1024, nfc_commands_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewTextBox);
}

bool nfc_tools_scene_nfc_commands_run_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventWriteSuccess ||
           event.event == NfcToolsEventWriteFail) {
            text_box_set_text(app->text_box, furi_string_get_cstr(app->info_str));
            consumed = true;
        }
    }

    return consumed;
}

void nfc_tools_scene_nfc_commands_run_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_commands_stop_worker(app);
    text_box_reset(app->text_box);
}
