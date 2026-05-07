#include "../include/nfc_tools_ntag.h"

// ── Exclusive formatting bound calculation ───────────────────────────────────

static uint16_t nfc_tools_ntag_format_end_page(MfUltralightType type) {
    switch(type) {
    case MfUltralightTypeOrigin:
        return 16; // NTAG210 / NTAG210u : 0x04–0x0F

    case MfUltralightTypeNTAG213:
        return 40; // NTAG213 / NTAG213 TT : 0x04–0x27

    case MfUltralightTypeNTAG215:
        return 130; // NTAG215 : 0x04–0x81

    case MfUltralightTypeNTAG216:
        return 226; // NTAG216 : 0x04–0xE1

    default:
        return 0; // unsupported type → formatting refused
    }
}

// ── Format ────────────────────────────────────────────────────────────────────

bool nfc_tools_ntag_format(NfcToolsApp* app) {
    MfUltralightVersion version = {};
    MfUltralightType type = MfUltralightTypeOrigin;
    if(mf_ultralight_poller_sync_read_version(app->nfc, &version) == MfUltralightErrorNone) {
        type = mf_ultralight_get_type_by_version(&version);
    }

    uint16_t end_page = nfc_tools_ntag_format_end_page(type);
    if(end_page == 0) {
        furi_string_set(
            app->info_str, "Unsupported type\nfor formatting\n(UL11/UL21/MfulC...)");
        return false;
    }

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;

    MfUltralightPage zero = {.data = {0x00, 0x00, 0x00, 0x00}};
    bool ok = true;

    for(uint16_t p = 4; p < end_page; p++) {
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
            ok = false;
            break;
        }
        if(mf_ultralight_poller_sync_write_page(app->nfc, p, &zero) !=
           MfUltralightErrorNone) {
            furi_string_printf(app->info_str, "Page error 0x%02X", (unsigned)p);
            ok = false;
            break;
        }
    }

    if(ok) {
        furi_string_printf(
            app->info_str,
            "Memory formatted!\n0x%02X-0x%02X (0x00)\nBack to exit",
            (unsigned)4,
            (unsigned)(end_page - 1));
    }

    return ok;
}

// ── Memory Dump ───────────────────────────────────────────────────────────────

bool nfc_tools_ntag_dump(NfcToolsApp* app) {
    MfUltralightData* mful = mf_ultralight_alloc();
    MfUltralightError err =
        mf_ultralight_poller_sync_read_card(app->nfc, mful, NULL);

    if(err != MfUltralightErrorNone) {
        furi_string_cat_printf(
            app->info_str, "Read error\ncode: %d\n", (int)err);
        mf_ultralight_free(mful);
        return false;
    }

    uint16_t pages_read  = mful->pages_read;
    uint16_t pages_total = mf_ultralight_get_pages_total(app->mful_type);

    furi_string_cat_printf(
        app->info_str,
        "%d/%d pages (4B)\n\n",
        pages_read,
        pages_total);
    furi_string_cat_printf(
        app->ndef_str,
        "%d/%d pages - ASCII\n\n",
        pages_read,
        pages_total);

    for(uint16_t p = 0; p < pages_total; p++) {
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) break;

        if(p < pages_read) {
            const uint8_t* d = mful->page[p].data;

            furi_string_cat_printf(
                app->info_str,
                "[%02X] %02X %02X %02X %02X\n",
                p, d[0], d[1], d[2], d[3]);

            furi_string_cat_printf(app->ndef_str, "[%02X] %c%c%c%c\n",
                p,
                (d[0] >= 32 && d[0] < 127) ? (char)d[0] : '.',
                (d[1] >= 32 && d[1] < 127) ? (char)d[1] : '.',
                (d[2] >= 32 && d[2] < 127) ? (char)d[2] : '.',
                (d[3] >= 32 && d[3] < 127) ? (char)d[3] : '.');
        } else {
            furi_string_cat_printf(app->info_str, "[%02X] ---\n", p);
            furi_string_cat_printf(app->ndef_str, "[%02X] ----\n", p);
        }
    }

    mf_ultralight_free(mful);
    return true;
}
