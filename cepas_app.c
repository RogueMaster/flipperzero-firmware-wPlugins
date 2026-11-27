/*
 * CEPAS Reader for Flipper Zero
 * Shows the balance and recent transactions of Singapore EZ-Link and
 * NETS FlashPay cards. Read-only: it never writes to the card.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <datetime/datetime.h>
#include <toolbox/bit_buffer.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>
#include <nfc/protocols/iso14443_4b/iso14443_4b_poller.h>

#include "cepas.h"
#include "cepas_reader_icons.h"

#define TAG "CEPAS"

#define DUMP_DIR  EXT_PATH("apps_data/cepas_reader")
#define DUMP_PATH EXT_PATH("apps_data/cepas_reader/last_read.txt")

#define RX_MAX        512
#define HISTORY_MAX   (CEPAS_MAX_TXNS * CEPAS_TXN_SIZE)
#define HIST_PER_PAGE 2
#define HIST_ROW_H    32

typedef enum {
    AppStateScanning,
    AppStateReading,
    AppStateResult,
    AppStateError,
} AppState;

typedef enum {
    AppEventInput,
    AppEventDetected,
    AppEventReadDone,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
    NfcProtocol protocol;
    NfcProtocol seen; /* first protocol detected, for the error message */
} AppEvent;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    NotificationApp* notifications;

    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;
    NfcProtocol protocol;
    bool detect_reported;

    /* Shown on screen (guarded by mutex) */
    AppState state;
    CepasCard card;
    char error[96];
    uint8_t page; /* 0 = summary, 1 = history list, 2 = transaction detail */
    size_t hist_sel;
    int32_t view_px; /* current scroll position of the history list */
    int32_t view_target; /* where the list is scrolling to */
    int32_t cursor_px; /* current position of the selection bar */
    uint32_t tick;

    /* Worker-side buffers (used only inside the NFC callback) */
    bool read_ok;
    bool quiet; /* suppress per-APDU logging during brute force */
    CepasCard work_card;
    char work_error[96];
    BitBuffer* tx;
    BitBuffer* rx;
    uint8_t resp[RX_MAX];
    uint8_t history[HISTORY_MAX];
    FuriString* dump;
} App;

/* ---------- read log (saved to the SD card for troubleshooting) ---------- */

static void dump_hex(App* app, const char* label, const uint8_t* data, size_t len) {
    FuriString* line = furi_string_alloc_printf("%s (%u):", label, (unsigned)len);
    for(size_t i = 0; i < len; i++)
        furi_string_cat_printf(line, " %02X", data[i]);
    furi_string_cat_printf(app->dump, "%s\n", furi_string_get_cstr(line));
    furi_string_free(line);
}

static void dump_text(App* app, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    FuriString* line = furi_string_alloc_vprintf(fmt, args);
    va_end(args);
    furi_string_cat_printf(app->dump, "%s\n", furi_string_get_cstr(line));
    furi_string_free(line);
}

static void save_dump(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, DUMP_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DUMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, furi_string_get_cstr(app->dump), furi_string_size(app->dump));
    } else {
        FURI_LOG_E(TAG, "Could not write %s", DUMP_PATH);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ---------- APDU exchange (NFC worker thread) ---------- */

static bool apdu(
    App* app,
    void* instance,
    const uint8_t* cmd,
    size_t cmd_len,
    size_t* resp_len,
    uint16_t* sw) {
    bit_buffer_reset(app->tx);
    bit_buffer_append_bytes(app->tx, cmd, cmd_len);
    bit_buffer_reset(app->rx);
    if(!app->quiet) dump_hex(app, ">>", cmd, cmd_len);

    int err;
    if(app->protocol == NfcProtocolIso14443_4b) {
        err = iso14443_4b_poller_send_block(instance, app->tx, app->rx);
    } else {
        err = iso14443_4a_poller_send_block(instance, app->tx, app->rx);
    }
    if(err != 0) {
        dump_text(app, "<< transceive error %d", err);
        return false;
    }

    size_t n = bit_buffer_get_size_bytes(app->rx);
    const uint8_t* data = bit_buffer_get_data(app->rx);
    if(!app->quiet || (n >= 2 && data[n - 2] != 0x67)) dump_hex(app, "<<", data, n);
    if(n < 2) return false;

    *sw = (uint16_t)((data[n - 2] << 8) | data[n - 1]);
    size_t body = n - 2;
    if(body > RX_MAX) body = RX_MAX;
    memcpy(app->resp, data, body);
    *resp_len = body;
    return true;
}

static void read_card(App* app, void* instance) {
    CepasCard* card = &app->work_card;
    memset(card, 0, sizeof(*card));
    app->read_ok = false;
    app->work_error[0] = '\0';

    size_t len = 0;
    uint16_t sw = 0;

    /* Select the CEPAS directory file. The reply (FCI) names the issuer:
     * NETS cards answer "NEST-CEPAS". */
    bool fci_nets = false;
    static const uint8_t select_cmd[] = {0x00, 0xA4, 0x00, 0x00, 0x02, 0x40, 0x00};
    if(apdu(app, instance, select_cmd, sizeof(select_cmd), &len, &sw)) {
        dump_text(app, "SELECT 4000 -> SW %04X", sw);
        for(size_t i = 0; i + 4 <= len; i++) {
            if(memcmp(&app->resp[i], "NEST", 4) == 0 || memcmp(&app->resp[i], "NETS", 4) == 0) {
                fci_nets = true;
            }
        }
    }

    /* Read Purse. EZ-Link accepts "90 32 03 00 00"; other issuers reject
     * that with 6700 (wrong length), so try other encodings in turn. */
    static const uint8_t purse_variants[][8] = {
        {5, 0x90, 0x32, CEPAS_PURSE_ID, 0x00, 0x00}, /* Le=00 */
        {6, 0x90, 0x32, CEPAS_PURSE_ID, 0x00, 0x00, 0x00}, /* FareBot form */
        {4, 0x90, 0x32, CEPAS_PURSE_ID, 0x00}, /* no Le */
    };
    bool got_purse = false;
    uint16_t first_sw = 0;
    bool any_answer = false;
    for(size_t v = 0; v < COUNT_OF(purse_variants) && !got_purse; v++) {
        const uint8_t* c = purse_variants[v];
        if(!apdu(app, instance, &c[1], c[0], &len, &sw)) continue;
        if(!any_answer) first_sw = sw;
        any_answer = true;
        if((sw & 0xFF00) == 0x6C00) {
            /* Card told us the exact length it wants. */
            uint8_t retry[5] = {0x90, 0x32, CEPAS_PURSE_ID, 0x00, (uint8_t)(sw & 0xFF)};
            if(!apdu(app, instance, retry, sizeof(retry), &len, &sw)) continue;
        }
        if(sw == 0x9000 && len >= CEPAS_PURSE_MIN_LEN && (len % CEPAS_TXN_SIZE) != 0) {
            dump_text(app, "Purse read OK with variant %u", (unsigned)v);
            got_purse = true;
        } else {
            dump_text(app, "Purse variant %u: SW %04X, %u bytes", (unsigned)v, sw, (unsigned)len);
        }
    }
    /* Last resort: some cards only accept the exact purse length as Le. */
    if(!got_purse && any_answer) {
        dump_text(app, "Trying every Le value...");
        app->quiet = true;
        for(unsigned le = CEPAS_PURSE_MIN_LEN; le <= 0xFF && !got_purse; le++) {
            uint8_t c[5] = {0x90, 0x32, CEPAS_PURSE_ID, 0x00, (uint8_t)le};
            if(!apdu(app, instance, c, sizeof(c), &len, &sw)) break;
            if(sw == 0x9000 && len >= CEPAS_PURSE_MIN_LEN) {
                dump_text(app, "Purse read OK with Le=%02X", le);
                got_purse = true;
            } else if(sw != 0x6700) {
                dump_text(app, "Le=%02X -> SW %04X, %u bytes", le, sw, (unsigned)len);
            }
        }
        app->quiet = false;
        if(!got_purse) dump_text(app, "No Le value worked");
    }

    if(got_purse && cepas_parse_purse(app->resp, len, &card->purse)) {
        card->has_purse = true;
        dump_hex(app, "PURSE", app->resp, len);
        dump_text(
            app,
            "Purse: ver %u status %02X balance %ld log_count %u",
            card->purse.version,
            card->purse.status,
            (long)card->purse.balance,
            card->purse.log_count);
    } else if(!any_answer) {
        snprintf(app->work_error, sizeof(app->work_error), "Card did not answer\nthe purse read.");
        return;
    } else {
        snprintf(
            app->work_error,
            sizeof(app->work_error),
            "Card refused the\npurse read (SW %04X)",
            first_sw);
    }
    card->issuer = card->has_purse ? cepas_issuer(&card->purse) : CepasIssuerUnknown;
    if(card->issuer == CepasIssuerUnknown && fci_nets) card->issuer = CepasIssuerNets;

    /* Transaction history. The card may return fewer records than asked,
     * so keep asking from the next record until we have them all (or, with
     * no purse, until the card runs out). If the card rejects the length
     * byte, retry without Le. */
    size_t want = card->has_purse ? card->purse.log_count : CEPAS_MAX_TXNS;
    if(want > CEPAS_MAX_TXNS) want = CEPAS_MAX_TXNS;
    size_t hist_len = 0;
    bool with_le = true;
    for(int attempt = 0; attempt < 10 && hist_len / CEPAS_TXN_SIZE < want; attempt++) {
        uint8_t hist_cmd[] = {
            0x90, 0x32, CEPAS_PURSE_ID, 0x00, 0x01, (uint8_t)(hist_len / CEPAS_TXN_SIZE), 0x00};
        if(!apdu(app, instance, hist_cmd, with_le ? 7 : 6, &len, &sw)) break;
        if(sw == 0x6700 && with_le && hist_len == 0) {
            dump_text(app, "History: retrying without Le");
            with_le = false;
            continue;
        }
        if(sw != 0x9000 || len < CEPAS_TXN_SIZE) {
            dump_text(app, "History stopped: SW %04X, %u bytes", sw, (unsigned)len);
            break;
        }
        size_t space = HISTORY_MAX - hist_len;
        size_t take = len - (len % CEPAS_TXN_SIZE);
        if(take > space) take = space;
        memcpy(&app->history[hist_len], app->resp, take);
        hist_len += take;
        if(!card->has_purse && take < 15 * CEPAS_TXN_SIZE) break; /* card ran out */
    }
    card->txn_count = cepas_parse_history(app->history, hist_len, card->txns, CEPAS_MAX_TXNS);
    dump_text(app, "History records: %u of %u", (unsigned)card->txn_count, (unsigned)want);
    if(!card->has_purse && card->txn_count == 0) return; /* keep the error */

    app->read_ok = true;
}

static NfcCommand poller_callback(NfcGenericEvent event, void* context) {
    App* app = context;
    bool ready = false;

    if(event.protocol == NfcProtocolIso14443_4b) {
        const Iso14443_4bPollerEvent* e = event.event_data;
        ready = (e->type == Iso14443_4bPollerEventTypeReady);
    } else if(event.protocol == NfcProtocolIso14443_4a) {
        const Iso14443_4aPollerEvent* e = event.event_data;
        ready = (e->type == Iso14443_4aPollerEventTypeReady);
    }
    if(!ready) return NfcCommandContinue;

    read_card(app, event.instance);

    AppEvent out = {.type = AppEventReadDone};
    furi_message_queue_put(app->queue, &out, 0);
    return NfcCommandStop;
}

static void scanner_callback(NfcScannerEvent event, void* context) {
    App* app = context;
    if(event.type != NfcScannerEventTypeDetected || app->detect_reported) return;

    NfcProtocol chosen = NfcProtocolInvalid;
    for(size_t i = 0; i < event.data.protocol_num; i++) {
        NfcProtocol p = event.data.protocols[i];
        if(p == NfcProtocolIso14443_4b || nfc_protocol_has_parent(p, NfcProtocolIso14443_4b)) {
            chosen = NfcProtocolIso14443_4b;
            break;
        }
        if(p == NfcProtocolIso14443_4a || nfc_protocol_has_parent(p, NfcProtocolIso14443_4a)) {
            chosen = NfcProtocolIso14443_4a;
        }
    }
    if(chosen == NfcProtocolInvalid && event.data.protocol_num > 0) {
        /* Report what we saw so the error screen can name it. */
        AppEvent out = {
            .type = AppEventDetected,
            .protocol = NfcProtocolInvalid,
            .seen = event.data.protocols[0]};
        app->detect_reported = true;
        furi_message_queue_put(app->queue, &out, 0);
        return;
    }
    app->detect_reported = true;
    AppEvent out = {.type = AppEventDetected, .protocol = chosen};
    furi_message_queue_put(app->queue, &out, 0);
}

/* ---------- scan control (main thread) ---------- */

static void start_scan(App* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = AppStateScanning;
    app->page = 0;
    app->hist_sel = 0;
    app->view_px = app->view_target = app->cursor_px = 0;
    furi_mutex_release(app->mutex);

    furi_string_reset(app->dump);
    app->detect_reported = false;
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, scanner_callback, app);
    notification_message(app->notifications, &sequence_blink_start_cyan);
}

static void stop_scanner(App* app) {
    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
        app->scanner = NULL;
    }
}

static void stop_poller(App* app) {
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

static void show_error(App* app, const char* msg) {
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_error);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    snprintf(app->error, sizeof(app->error), "%s", msg);
    app->state = AppStateError;
    furi_mutex_release(app->mutex);
}

static void on_detected(App* app, const AppEvent* ev) {
    if(app->state != AppStateScanning) return;
    stop_scanner(app);

    if(ev->protocol == NfcProtocolInvalid) {
        NfcProtocol seen = ev->seen;
        char msg[96];
        snprintf(
            msg,
            sizeof(msg),
            "Not a CEPAS card.\nDetected: %s",
            nfc_device_get_protocol_name(seen));
        dump_text(app, "Unsupported card: %s", nfc_device_get_protocol_name(seen));
        save_dump(app);
        show_error(app, msg);
        return;
    }

    app->protocol = ev->protocol;
    dump_text(app, "Detected %s", nfc_device_get_protocol_name(ev->protocol));
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->state = AppStateReading;
    furi_mutex_release(app->mutex);

    app->poller = nfc_poller_alloc(app->nfc, app->protocol);
    nfc_poller_start(app->poller, poller_callback, app);
}

static void on_read_done(App* app) {
    if(app->poller) {
        const NfcDeviceData* data = nfc_poller_get_data(app->poller);
        if(data) {
            size_t uid_len = 0;
            const uint8_t* uid = NULL;
            if(app->protocol == NfcProtocolIso14443_4b) {
                uid = iso14443_4b_get_uid((const Iso14443_4bData*)data, &uid_len);
            } else {
                uid = iso14443_4a_get_uid((const Iso14443_4aData*)data, &uid_len);
            }
            if(uid && uid_len) dump_hex(app, "UID", uid, uid_len);
        }
    }
    stop_poller(app);
    save_dump(app);

    if(!app->read_ok) {
        show_error(app, app->work_error[0] ? app->work_error : "Read failed.");
        return;
    }
    notification_message(app->notifications, &sequence_blink_stop);
    notification_message(app->notifications, &sequence_success);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->card = app->work_card;
    app->state = AppStateResult;
    app->page = 0;
    app->hist_sel = 0;
    app->view_px = app->view_target = app->cursor_px = 0;
    furi_mutex_release(app->mutex);
}

/* ---------- drawing ---------- */

static const char* const month_names[] =
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/* "19 Mar 2020" */
static void format_date(uint32_t wallclock, char* out, size_t n) {
    DateTime dt;
    datetime_timestamp_to_datetime(wallclock, &dt);
    const char* mon = (dt.month >= 1 && dt.month <= 12) ? month_names[dt.month - 1] : "???";
    snprintf(out, n, "%u %s %04u", dt.day, mon, dt.year);
}

/* "16:41" */
static void format_time(uint32_t wallclock, char* out, size_t n) {
    DateTime dt;
    datetime_timestamp_to_datetime(wallclock, &dt);
    snprintf(out, n, "%02u:%02u", dt.hour, dt.minute);
}

/* Draws text cut down with "..." so it fits in max_w pixels. */
static void draw_fit(Canvas* canvas, int32_t x, int32_t y, const char* text, uint16_t max_w) {
    char buf[64];
    size_t len = strlen(text);
    if(len > 60) len = 60;
    memcpy(buf, text, len);
    buf[len] = '\0';
    if(canvas_string_width(canvas, buf) > max_w) {
        while(len > 0) {
            len--;
            buf[len] = '.';
            buf[len + 1] = '.';
            buf[len + 2] = '.';
            buf[len + 3] = '\0';
            if(canvas_string_width(canvas, buf) <= max_w) break;
        }
    }
    canvas_draw_str(canvas, x, y, buf);
}

static void draw_summary(Canvas* canvas, const CepasCard* card) {
    const CepasPurse* p = &card->purse;
    char buf[40];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, cepas_issuer_name(card->issuer));

    if(card->has_purse) {
        /* Balance, centred: small "$" + big digits */
        int32_t bal = p->balance;
        uint32_t mag = bal < 0 ? (uint32_t)(-(int64_t)bal) : (uint32_t)bal;
        const char* sign = bal < 0 ? "-$" : "$";
        snprintf(
            buf, sizeof(buf), "%lu.%02lu", (unsigned long)(mag / 100), (unsigned long)(mag % 100));
        canvas_set_font(canvas, FontPrimary);
        uint16_t sw = canvas_string_width(canvas, sign);
        canvas_set_font(canvas, FontBigNumbers);
        uint16_t nw = canvas_string_width(canvas, buf);
        int32_t x = (128 - (int32_t)(sw + 2 + nw)) / 2;
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x, 33, sign);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, x + sw + 2, 34, buf);

        canvas_set_font(canvas, FontSecondary);
        cepas_format_can(p->can, buf, sizeof(buf));
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, buf);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignBottom, "Balance unavailable");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, "History only");
    }

    /* Bottom row: expiry left, history hint right */
    canvas_set_font(canvas, FontSecondary);
    if(card->has_purse) {
        DateTime dt;
        datetime_timestamp_to_datetime(cepas_days_to_wallclock(p->expiry_days), &dt);
        const char* mon = (dt.month >= 1 && dt.month <= 12) ? month_names[dt.month - 1] : "???";
        snprintf(buf, sizeof(buf), "Exp %s %04u", mon, dt.year);
        canvas_draw_str(canvas, 0, 61, buf);
    }
    if(card->txn_count)
        canvas_draw_str_aligned(canvas, 128, 61, AlignRight, AlignBottom, "History >");
}

static void draw_history(Canvas* canvas, const App* app) {
    const CepasCard* card = &app->card;
    int32_t first = app->view_px / HIST_ROW_H;
    for(int32_t idx = first; idx <= first + HIST_PER_PAGE && idx < (int32_t)card->txn_count;
        idx++) {
        const CepasTxn* t = &card->txns[idx];
        int32_t yb = idx * HIST_ROW_H - app->view_px;
        CepasTxnText tx;
        char amt[16], date[16], time[8];
        cepas_txn_describe(t, &tx);
        cepas_format_amount(t->amount, true, amt, sizeof(amt));
        uint32_t wc = cepas_time_to_wallclock(t->time);
        format_date(wc, date, sizeof(date));
        format_time(wc, time, sizeof(time));

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 3, yb + 10, tx.kind);
        canvas_draw_str_aligned(canvas, 121, yb + 10, AlignRight, AlignBottom, amt);
        canvas_set_font(canvas, FontSecondary);
        draw_fit(canvas, 3, yb + 20, tx.detail[0] ? tx.detail : "-", 118);
        canvas_draw_str(canvas, 3, yb + 29, date);
        canvas_draw_str_aligned(canvas, 121, yb + 29, AlignRight, AlignBottom, time);
    }

    /* Selection bar, drawn in XOR so text under it turns white even while
     * it slides between entries. Pieces don't overlap (XOR-safe) and give
     * slightly rounded corners. */
    int32_t y = app->cursor_px - app->view_px;
    int32_t h = HIST_ROW_H - 1;
    canvas_set_color(canvas, ColorXOR);
    canvas_draw_box(canvas, 1, y, 122, h);
    canvas_draw_box(canvas, 0, y + 1, 1, h - 2);
    canvas_draw_box(canvas, 123, y + 1, 1, h - 2);
    canvas_set_color(canvas, ColorBlack);

    if(card->txn_count > 1) {
        elements_scrollbar_pos(canvas, 128, 0, 64, app->hist_sel, card->txn_count);
    }
}

static void draw_detail(Canvas* canvas, const CepasCard* card, size_t sel) {
    if(sel >= card->txn_count) return;
    const CepasTxn* t = &card->txns[sel];
    CepasTxnText tx;
    char amt[16], date[16], time[8], buf[48];
    cepas_txn_describe(t, &tx);
    cepas_format_amount(t->amount, true, amt, sizeof(amt));
    uint32_t wc = cepas_time_to_wallclock(t->time);
    format_date(wc, date, sizeof(date));
    format_time(wc, time, sizeof(time));

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, 10, tx.kind);
    canvas_draw_str_aligned(canvas, 128, 10, AlignRight, AlignBottom, amt);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);
    if(tx.from[0]) {
        snprintf(buf, sizeof(buf), "From: %s", tx.from);
        draw_fit(canvas, 0, 23, buf, 128);
        snprintf(buf, sizeof(buf), "To:   %s", tx.to);
        draw_fit(canvas, 0, 33, buf, 128);
    } else {
        draw_fit(canvas, 0, 23, tx.detail[0] ? tx.detail : "-", 128);
        if(t->user_data[0]) {
            snprintf(buf, sizeof(buf), "Code: %s", t->user_data);
            canvas_draw_str(canvas, 0, 33, buf);
        }
    }
    canvas_draw_str(canvas, 0, 45, date);
    canvas_draw_str_aligned(canvas, 128, 45, AlignRight, AlignBottom, time);

    snprintf(buf, sizeof(buf), "%u of %u", (unsigned)(sel + 1), (unsigned)card->txn_count);
    canvas_draw_str(canvas, 0, 61, buf);
    snprintf(buf, sizeof(buf), "Type %02X", t->type);
    canvas_draw_str_aligned(canvas, 128, 61, AlignRight, AlignBottom, buf);
}

static void draw_callback(Canvas* canvas, void* context) {
    App* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);

    switch(app->state) {
    case AppStateScanning:
    case AppStateReading: {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignBottom, "CEPAS Reader");
        canvas_draw_icon(canvas, 4, 22, &I_card_30x17);
        canvas_set_font(canvas, FontSecondary);
        if(app->state == AppStateScanning) {
            canvas_draw_str(canvas, 42, 28, "Hold EZ-Link or");
            canvas_draw_str(canvas, 42, 38, "NETS card to the");
            canvas_draw_str(canvas, 42, 48, "back of Flipper");
        } else {
            canvas_draw_str(canvas, 42, 33, "Reading card...");
            canvas_draw_str(canvas, 42, 43, "Keep it still");
        }
        char dots[4] = "   ";
        for(uint32_t i = 0; i < (app->tick % 4); i++)
            dots[i] = '.';
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, dots);
        break;
    }
    case AppStateResult:
        if(app->page == 0) {
            draw_summary(canvas, &app->card);
        } else if(app->page == 1) {
            draw_history(canvas, app);
        } else {
            draw_detail(canvas, &app->card, app->hist_sel);
        }
        break;
    case AppStateError:
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 0, 10, "Couldn't read card");
        canvas_set_font(canvas, FontSecondary);
        elements_multiline_text(canvas, 0, 24, app->error);
        canvas_draw_str(canvas, 0, 61, "OK: try again");
        break;
    }
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* input, void* context) {
    App* app = context;
    AppEvent ev = {.type = AppEventInput, .input = *input};
    furi_message_queue_put(app->queue, &ev, 0);
}

/* Keep the selected entry on screen: scroll only when it would leave. */
static void hist_follow(App* app) {
    int32_t cy = (int32_t)app->hist_sel * HIST_ROW_H;
    if(cy < app->view_target) app->view_target = cy;
    if(cy + HIST_ROW_H > app->view_target + 64) app->view_target = cy + HIST_ROW_H - 64;
    if(app->view_target < 0) app->view_target = 0;
}

/* Jump the animation to its end state (used when the list first appears). */
static void hist_snap(App* app) {
    hist_follow(app);
    app->view_px = app->view_target;
    app->cursor_px = (int32_t)app->hist_sel * HIST_ROW_H;
}

/* Jumps straight to the target: the Flipper's LCD is too slow for smooth
 * motion without ghosting. */
static bool anim_move(int32_t* cur, int32_t target) {
    *cur = target;
    return false;
}

/* Advances the slide animation one frame. Returns true while moving. */
static bool hist_animate(App* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool moving = false;
    if(app->state == AppStateResult && app->page == 1) {
        moving |= anim_move(&app->cursor_px, (int32_t)app->hist_sel * HIST_ROW_H);
        moving |= anim_move(&app->view_px, app->view_target);
    }
    furi_mutex_release(app->mutex);
    return moving;
}

/* Returns false when the app should exit. */
static bool handle_input(App* app, const InputEvent* in) {
    if(in->type != InputTypeShort && in->type != InputTypeRepeat) return true;

    switch(app->state) {
    case AppStateScanning:
    case AppStateReading:
        if(in->key == InputKeyBack) return false;
        break;

    case AppStateError:
        if(in->key == InputKeyBack) return false;
        if(in->key == InputKeyOk) start_scan(app);
        break;

    case AppStateResult: {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        size_t count = app->card.txn_count;
        bool exit = false, rescan = false;
        InputKey k = in->key;
        bool shortp = (in->type == InputTypeShort);
        if(app->page == 0) {
            if(k == InputKeyBack) exit = true;
            if(k == InputKeyOk && shortp) rescan = true;
            if((k == InputKeyRight || k == InputKeyDown) && count) {
                app->page = 1;
                hist_snap(app);
            }
        } else if(app->page == 1) {
            if(k == InputKeyBack || k == InputKeyLeft) app->page = 0;
            if(k == InputKeyDown && app->hist_sel + 1 < count) app->hist_sel++;
            if(k == InputKeyUp) {
                if(app->hist_sel > 0)
                    app->hist_sel--;
                else
                    app->page = 0;
            }
            hist_follow(app);
            if((k == InputKeyOk || k == InputKeyRight) && shortp) app->page = 2;
        } else {
            if(k == InputKeyDown && app->hist_sel + 1 < count) app->hist_sel++;
            if(k == InputKeyUp && app->hist_sel > 0) app->hist_sel--;
            if(k == InputKeyBack || k == InputKeyLeft || (k == InputKeyOk && shortp)) {
                app->page = 1;
                hist_snap(app);
            }
        }
        furi_mutex_release(app->mutex);
        if(exit) return false;
        if(rescan) start_scan(app);
        break;
    }
    }
    return true;
}

int32_t cepas_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->tx = bit_buffer_alloc(64);
    app->rx = bit_buffer_alloc(RX_MAX);
    app->dump = furi_string_alloc();
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->nfc = nfc_alloc();
    start_scan(app);

    bool running = true;
    bool animating = false;
    AppEvent ev;
    while(running) {
        if(furi_message_queue_get(app->queue, &ev, animating ? 40 : 250) == FuriStatusOk) {
            switch(ev.type) {
            case AppEventInput:
                running = handle_input(app, &ev.input);
                break;
            case AppEventDetected:
                on_detected(app, &ev);
                break;
            case AppEventReadDone:
                on_read_done(app);
                break;
            }
        }
        animating = hist_animate(app);
        app->tick++;
        view_port_update(app->view_port);
    }

    stop_scanner(app);
    stop_poller(app);
    notification_message(app->notifications, &sequence_blink_stop);

    nfc_free(app->nfc);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_string_free(app->dump);
    bit_buffer_free(app->rx);
    bit_buffer_free(app->tx);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->queue);
    free(app);
    return 0;
}
