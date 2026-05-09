#include "../../include/nfc_tools_i.h"
#include "../../include/nfc_tools_ndef.h"
#include "../../include/nfc_tools_mfc.h"
#include "../../include/nfc_tools_icode.h"
#include "../../include/nfc_tools_desfire.h"
#include <nfc/protocols/mf_ultralight/mf_ultralight_poller_sync.h>

// Delay between the first write attempt and the silent retry.
#define WRITE_RETRY_DELAY_MS 200u

// ── Callback NfcScanner ────────────────────────────────────────────────────

static void nfc_tools_ndef_scan_callback(NfcScannerEvent event, void* context) {
    NfcToolsApp* app = context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        app->detected_protocol = event.data.protocols[0];
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_DETECTED);
    }
}

// ── MF Ultralight / NTAG page write helper ────────────────────────────────
//
// Writes `ndef` (ndef_size bytes) page by page starting at page 4.
// Returns true on success, false on communication error or STOP request.
// Extracted from the worker so the same logic can be called twice for retry
// without code duplication.

static bool write_mful_pages(NfcToolsApp* app, const uint8_t* ndef, size_t ndef_size) {
    size_t num_pages = (ndef_size + MF_ULTRALIGHT_PAGE_SIZE - 1) / MF_ULTRALIGHT_PAGE_SIZE;

    for(size_t i = 0; i < num_pages; i++) {
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;

        MfUltralightPage page = {.data = {0, 0, 0, 0}};
        size_t offset = i * MF_ULTRALIGHT_PAGE_SIZE;
        size_t chunk  = ndef_size - offset;
        if(chunk > MF_ULTRALIGHT_PAGE_SIZE) chunk = MF_ULTRALIGHT_PAGE_SIZE;
        memcpy(page.data, ndef + offset, chunk);

        if(mf_ultralight_poller_sync_write_page(app->nfc, (uint16_t)(4 + i), &page) !=
           MfUltralightErrorNone) {
            furi_string_set(app->info_str, NTS_ERR_WRITE);
            return false;
        }
    }
    return true;
}

// ── Worker ─────────────────────────────────────────────────────────────────

static int32_t nfc_tools_ndef_write_worker(void* context) {
    NfcToolsApp* app = context;

    // Phase 1: build the NDEF buffer before touching NFC
    size_t   ndef_size = 0;
    uint8_t* ndef      = nfc_tools_ndef_build(app, &ndef_size);

    // Phase 2: detect a tag via NfcScanner (15 s timeout)
    NfcScanner* scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(scanner, nfc_tools_ndef_scan_callback, app);

    uint32_t flags = furi_event_flag_wait(
        app->worker_flags,
        NFC_TOOLS_WORKER_FLAG_DETECTED | NFC_TOOLS_WORKER_FLAG_STOP,
        FuriFlagWaitAny,
        15000);

    nfc_scanner_stop(scanner);
    nfc_scanner_free(scanner);

    if(flags & NFC_TOOLS_WORKER_FLAG_STOP) {
        free(ndef);
        return 0;
    }

    if(flags == (uint32_t)FuriFlagErrorTimeout) {
        free(ndef);
        notification_message(app->notifications, &sequence_error);
        furi_string_set(app->info_str, NTS_ERR_NO_TAG);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    // Phase 3: verify that the tag is supported
    bool is_mful     = (app->detected_protocol == NfcProtocolMfUltralight);
    bool is_iso15693 = (app->detected_protocol == NfcProtocolIso15693_3 ||
                        app->detected_protocol == NfcProtocolSlix);
    bool is_mfc      = (app->detected_protocol == NfcProtocolMfClassic);
    bool is_desfire  = (app->detected_protocol == NfcProtocolMfDesfire);

    if(!is_mful && !is_iso15693 && !is_mfc && !is_desfire) {
        free(ndef);
        notification_message(app->notifications, &sequence_error);
        furi_string_printf(
            app->info_str,
            "Unsupported tag:\n%s",
            nfc_device_get_protocol_name(app->detected_protocol));
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
        free(ndef);
        return 0;
    }

    // ── Retry helper ───────────────────────────────────────────────────────
    // After a transient write failure, wait WRITE_RETRY_DELAY_MS then try
    // once more — silently, with no intermediate UI update.
    // The STOP flag is checked both before the delay and after it so that a
    // user cancellation during the 200 ms window is handled immediately.
#define RETRY_WRITE(ok_var, write_expr)                                                   \
    do {                                                                                   \
        if(!(ok_var) &&                                                                    \
           !(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP)) {      \
            furi_delay_ms(WRITE_RETRY_DELAY_MS);                                          \
            if(!(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP)) {  \
                (ok_var) = (write_expr);                                                   \
            }                                                                              \
        }                                                                                  \
    } while(0)

    // ── ISO 15693 path (ICODE SLI / SLIX / SLIX2) ──────────────────────────
    if(is_iso15693) {
        bool use_slix = (app->detected_protocol == NfcProtocolSlix);
        bool ok = nfc_tools_icode_write_ndef(app, ndef, ndef_size, use_slix);
        RETRY_WRITE(ok, nfc_tools_icode_write_ndef(app, ndef, ndef_size, use_slix));
        free(ndef);

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        if(ok) {
            notification_message(app->notifications, &sequence_success);
            if(app->ndef_type == NdefTypeEmpty)
                furi_string_set(app->info_str, NTS_STATUS_TAG_ERASED);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
        } else {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        }
        return 0;
    }

    // ── Mifare Classic 1K / 4K path ─────────────────────────────────────────
    if(is_mfc) {
        bool ok = nfc_tools_mfc_write_ndef(app, ndef, ndef_size);
        RETRY_WRITE(ok, nfc_tools_mfc_write_ndef(app, ndef, ndef_size));
        free(ndef);

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        if(ok) {
            notification_message(app->notifications, &sequence_success);
            if(app->ndef_type == NdefTypeEmpty)
                furi_string_set(app->info_str, NTS_STATUS_TAG_ERASED);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
        } else {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        }
        return 0;
    }

    // ── MF DESFire path (NFC Forum Type 4 Tag) ──────────────────────────────
    if(is_desfire) {
        bool ok = nfc_tools_desfire_write_ndef(app, ndef, ndef_size);
        RETRY_WRITE(ok, nfc_tools_desfire_write_ndef(app, ndef, ndef_size));
        free(ndef);

        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

        if(ok) {
            notification_message(app->notifications, &sequence_success);
            if(app->ndef_type == NdefTypeEmpty)
                furi_string_set(app->info_str, NTS_STATUS_TAG_ERASED);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
        } else {
            notification_message(app->notifications, &sequence_error);
            view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        }
        return 0;
    }

    // ── MF Ultralight / NTAG path ────────────────────────────────────────────

    // Phase 4: read the Capability Container (page 3) to determine the user
    // capacity of the tag. This is a deterministic gate — a "tag too small"
    // error is not a transient failure and is not retried.
    MfUltralightPage cc_page;
    uint16_t capacity = 48; // conservative fallback (6 × 8 bytes)
    if(mf_ultralight_poller_sync_read_page(app->nfc, 3, &cc_page) == MfUltralightErrorNone) {
        if(cc_page.data[0] == 0xE1) {
            capacity = (uint16_t)cc_page.data[2] * 8;
        }
    }

    if(ndef_size > capacity) {
        notification_message(app->notifications, &sequence_error);
        furi_string_printf(
            app->info_str,
            "Tag too small!\n%u bytes required\n%u bytes available",
            (unsigned)ndef_size,
            (unsigned)capacity);
        free(ndef);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    // Phase 5: write pages — one silent retry on transient communication error.
    bool write_ok = write_mful_pages(app, ndef, ndef_size);
    RETRY_WRITE(write_ok, write_mful_pages(app, ndef, ndef_size));
    free(ndef);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(write_ok) {
        notification_message(app->notifications, &sequence_success);
        if(app->ndef_type == NdefTypeEmpty) {
            furi_string_set(app->info_str, NTS_STATUS_TAG_ERASED);
        } else {
            furi_string_printf(
                app->info_str, "%u bytes written\nBack to exit", (unsigned)ndef_size);
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
    } else {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    }

    return 0;
}

#undef RETRY_WRITE

// ── Stop helper ────────────────────────────────────────────────────────────

static void nfc_tools_ndef_write_stop_worker(NfcToolsApp* app) {
    if(app->worker_thread) {
        furi_event_flag_set(app->worker_flags, NFC_TOOLS_WORKER_FLAG_STOP);
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        furi_event_flag_free(app->worker_flags);
        app->worker_thread = NULL;
        app->worker_flags  = NULL;
    }
}

// ── Scene ──────────────────────────────────────────────────────────────────

void nfc_tools_scene_write_scan_on_enter(void* context) {
    NfcToolsApp* app = context;

    popup_reset(app->popup);
    popup_set_header(
        app->popup,
        nfc_tools_ndef_write_label(app->ndef_type),
        64, 10, AlignCenter, AlignCenter);
    popup_set_text(
        app->popup,
        NTS_POPUP_APPROACH_TAG,
        64, 35, AlignCenter, AlignCenter);

    furi_string_reset(app->info_str);

    app->worker_flags  = furi_event_flag_alloc();
    app->worker_thread = furi_thread_alloc_ex(
        "NfcToolsNdefWrite", 4 * 1024, nfc_tools_ndef_write_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
}

bool nfc_tools_scene_write_scan_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app      = context;
    bool         consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventWriteSuccess) {
            popup_set_header(
                app->popup, NTS_STATUS_WRITE_COMPLETE, 64, 10, AlignCenter, AlignCenter);
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

void nfc_tools_scene_write_scan_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_tools_ndef_write_stop_worker(app);
    popup_reset(app->popup);
}
