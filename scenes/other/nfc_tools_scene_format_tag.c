#include "../../include/nfc_tools_i.h"
#include "../../include/nfc_tools_ntag.h"
#include "../../include/nfc_tools_icode.h"
#include "../../include/nfc_tools_mfc.h"

// ── Callback NfcScanner ─────────────────────────────────────────────────────

static void nfc_tools_format_scan_callback(NfcScannerEvent event, void* context) {
    NfcToolsApp* app = context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        app->detected_protocol = event.data.protocols[0];
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_DETECTED);
    }
}

// ── Worker ──────────────────────────────────────────────────────────────────

static int32_t nfc_tools_format_worker(void* context) {
    NfcToolsApp* app = context;

    // Phase 1: detect tag via NfcScanner
    NfcScanner* scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(scanner, nfc_tools_format_scan_callback, app);

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

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    // Phase 2: tag detected → switch display to "Formatting..."
    view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventScanSuccess);

    // ── MF Ultralight / NTAG ─────────────────────────────────────────────────
    if(app->detected_protocol == NfcProtocolMfUltralight) {
        bool ok = nfc_tools_ntag_format(app);

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        if(ok) {
            notification_message(app->notifications, &sequence_success);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
        } else {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        }

    // ── ISO 15693 (ICODE SLI / SLIX / SLIX2 et compatibles) ─────────────────
    } else if(app->detected_protocol == NfcProtocolIso15693_3 ||
              app->detected_protocol == NfcProtocolSlix) {

        bool use_slix = (app->detected_protocol == NfcProtocolSlix);
        bool ok = nfc_tools_icode_format(app, use_slix);

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        if(ok) {
            notification_message(app->notifications, &sequence_success);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
        } else {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        }

    // ── Mifare Classic 1K / 4K ───────────────────────────────────────────────
    } else if(app->detected_protocol == NfcProtocolMfClassic) {
        bool ok = nfc_tools_mfc_format(app);

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        if(ok) {
            notification_message(app->notifications, &sequence_success);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
        } else {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        }

    // ── DESFire (formatting not supported) ──────────────────────────────────
    } else if(app->detected_protocol == NfcProtocolMfDesfire) {
        notification_message(app->notifications, &sequence_error);
        furi_string_set(
            app->info_str,
            "DESFire detected\nFormatting not\nsupported");
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);

    // ── Unsupported protocol ─────────────────────────────────────────────────
    } else {
        notification_message(app->notifications, &sequence_error);
        furi_string_printf(
            app->info_str,
            "Unsupported tag:\n%s",
            nfc_device_get_protocol_name(app->detected_protocol));
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    }

    return 0;
}

// ── Stop helper ─────────────────────────────────────────────────────────────

static void nfc_tools_format_stop_worker(NfcToolsApp* app) {
    if(app->worker_thread) {
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_STOP);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        furi_event_flag_free(app->worker_flags);
        app->worker_thread = NULL;
        app->worker_flags  = NULL;
    }
}

// ── Scene ───────────────────────────────────────────────────────────────────

void nfc_tools_scene_format_tag_on_enter(void* context) {
    NfcToolsApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, NTS_POPUP_FORMAT_MEMORY, 64, 10, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        NTS_POPUP_FORMAT_ALL,
        64, 35, AlignCenter, AlignCenter);

    furi_string_reset(app->info_str);

    app->worker_flags = furi_event_flag_alloc();
    app->worker_thread =
        furi_thread_alloc_ex("NfcToolsFormat", 4 * 1024, nfc_tools_format_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
}

bool nfc_tools_scene_format_tag_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventScanSuccess) {
            popup_set_header(
                app->popup, NTS_POPUP_FORMATTING, 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup, NTS_POPUP_HOLD_TAG, 64, 35, AlignCenter, AlignCenter);
            consumed = true;
        } else if(event.event == NfcToolsEventWriteSuccess) {
            popup_set_header(app->popup, NTS_STATUS_FORMATTED, 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                furi_string_get_cstr(app->info_str),
                64, 35, AlignCenter, AlignCenter);
            consumed = true;
        } else if(event.event == NfcToolsEventWriteFail) {
            popup_set_header(app->popup, NTS_ERR_FAILED, 64, 10, AlignCenter, AlignCenter);
            popup_set_text(
                app->popup,
                furi_string_get_cstr(app->info_str),
                64, 35, AlignCenter, AlignCenter);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_tools_scene_format_tag_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_tools_format_stop_worker(app);
    popup_reset(app->popup);
}
