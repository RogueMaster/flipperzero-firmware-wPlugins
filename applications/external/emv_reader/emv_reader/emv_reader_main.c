#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stream/file_stream.h>

#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a.h>
#include <nfc/protocols/iso14443_4a/iso14443_4a_poller.h>

#include "ber_tlv.h"
#include "emv_apdu.h"

#define TAG "EmvReader"
#define APDU_BUF_SIZE 264
#define RECORDS_BUF_SIZE 4096
#define SAVE_DIR "/ext/apps_data/emv_reader"
#define MAX_AIDS 8
#define MAX_CVM_RULES 8

typedef enum {
    UiStateIdle,
    UiStateScanning,
    UiStateDone,
    UiStateError,
} UiState;

typedef enum {
    PageSummary = 0,
    PageDetails,
    PageCvm,
    PageAids,
    PageHex,
    PageCount,
} ResultPage;

typedef struct {
    uint8_t aid[16];
    uint8_t aid_len;
} AidEntry;

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* input_queue;

    Nfc* nfc;
    NfcPoller* poller;

    UiState ui_state;
    ResultPage page;
    FuriMutex* state_mutex;

    char card_label[24];
    char pan_str[24];
    char expiry_str[8];
    char holder_str[28];
    char track2_str[48];
    char err_msg[64];

    uint8_t aid[16];
    uint8_t aid_len;
    AidEntry aids[MAX_AIDS];
    uint8_t aids_count;

    uint8_t afl[252];
    size_t afl_len;
    uint8_t records[RECORDS_BUF_SIZE];
    size_t records_len;

    uint8_t aip[2];
    bool aip_present;
    char aip_decoded[40];

    uint8_t atc[2];
    bool atc_present;

    uint8_t cvm_list_buf[64];
    size_t cvm_list_len;
    EmvCvmRule cvm_rules[MAX_CVM_RULES];
    size_t cvm_rules_count;
    uint32_t cvm_amount_x;
    uint32_t cvm_amount_y;

    char service_code[4];
    char service_code_desc[24];

    EmvPinAnalysis pin_analysis;
    char pin_status_str[24];

    bool poll_done;
    bool poll_success;
    FuriEventFlag* result_flag;

    int hex_scroll;
    int aids_scroll;

    bool exit_requested;
} EmvReaderApp;

static void emv_reader_reset_card(EmvReaderApp* app) {
    app->card_label[0] = '\0';
    app->pan_str[0] = '\0';
    app->expiry_str[0] = '\0';
    app->holder_str[0] = '\0';
    app->track2_str[0] = '\0';
    app->err_msg[0] = '\0';
    app->aid_len = 0;
    app->aids_count = 0;
    app->afl_len = 0;
    app->records_len = 0;
    app->aip_present = false;
    app->aip_decoded[0] = '\0';
    app->atc_present = false;
    app->cvm_list_len = 0;
    app->cvm_rules_count = 0;
    app->cvm_amount_x = 0;
    app->cvm_amount_y = 0;
    app->service_code[0] = '\0';
    app->service_code_desc[0] = '\0';
    app->pin_status_str[0] = '\0';
    app->pin_analysis.status = EmvPinUnknown;
    app->pin_analysis.threshold = 0;
    app->poll_done = false;
    app->poll_success = false;
    app->hex_scroll = 0;
    app->aids_scroll = 0;
    app->page = PageSummary;
}

static void draw_callback(Canvas* canvas, void* ctx) {
    EmvReaderApp* app = ctx;
    furi_mutex_acquire(app->state_mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    switch(app->ui_state) {
    case UiStateIdle:
        canvas_draw_str(canvas, 2, 12, "EMV Reader");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "Read your own EMV cards.");
        canvas_draw_str(canvas, 2, 40, "Display + dump full APDU.");
        canvas_draw_str(canvas, 2, 56, "OK = Scan   BACK = Exit");
        break;

    case UiStateScanning:
        canvas_draw_str(canvas, 2, 12, "Scanning...");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, "Tap card to back of");
        canvas_draw_str(canvas, 2, 40, "Flipper Zero.");
        canvas_draw_str(canvas, 2, 56, "BACK = Cancel");
        break;

    case UiStateDone: {
        canvas_set_font(canvas, FontSecondary);
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "%d/%d", app->page + 1, PageCount);
        canvas_draw_str(canvas, 100, 8, hdr);

        char line[64];

        switch(app->page) {
        case PageSummary: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 10, app->card_label[0] ? app->card_label : "Card");
            canvas_set_font(canvas, FontSecondary);
            if(app->pan_str[0]) {
                snprintf(line, sizeof(line), "PAN %s", app->pan_str);
                canvas_draw_str(canvas, 2, 22, line);
            }
            if(app->expiry_str[0]) {
                snprintf(line, sizeof(line), "Exp %s", app->expiry_str);
                canvas_draw_str(canvas, 2, 32, line);
            }
            if(app->holder_str[0] && app->holder_str[0] != ' ') {
                snprintf(line, sizeof(line), "%s", app->holder_str);
                canvas_draw_str(canvas, 2, 42, line);
            }
            canvas_draw_str(canvas, 2, 53, "L/R pages  OK=again");
            break;
        }
        case PageDetails: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 10, "Details");
            canvas_set_font(canvas, FontSecondary);
            int y = 19;
            if(app->service_code[0]) {
                snprintf(line, sizeof(line), "SC %s %s", app->service_code, app->service_code_desc);
                canvas_draw_str(canvas, 2, y, line); y += 9;
            }
            if(app->aip_present) {
                snprintf(line, sizeof(line), "AIP %02X%02X %s",
                    app->aip[0], app->aip[1],
                    app->aip_decoded[0] ? app->aip_decoded : "");
                canvas_draw_str(canvas, 2, y, line); y += 9;
            }
            if(app->atc_present) {
                uint16_t atc = ((uint16_t)app->atc[0] << 8) | app->atc[1];
                snprintf(line, sizeof(line), "ATC %u", atc);
                canvas_draw_str(canvas, 2, y, line); y += 9;
            }
            if(app->pin_status_str[0]) {
                snprintf(line, sizeof(line), "PIN %s", app->pin_status_str);
                canvas_draw_str(canvas, 2, y, line); y += 9;
                if(app->pin_analysis.status == EmvPinDeferredOnline ||
                   app->pin_analysis.status == EmvPinNever) {
                    canvas_draw_str(canvas, 2, y, "(terminal limit applies)"); y += 9;
                }
            }
            if(y == 19) canvas_draw_str(canvas, 2, 22, "(no fields decoded)");
            canvas_draw_str(canvas, 70, 63, "L/R pages");
            break;
        }
        case PageCvm: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 10, "CVM list");
            canvas_set_font(canvas, FontSecondary);
            if(app->cvm_rules_count == 0) {
                canvas_draw_str(canvas, 2, 22, "(no CVM list found)");
            } else {
                snprintf(line, sizeof(line), "X=%lu Y=%lu (cents)",
                    (unsigned long)app->cvm_amount_x, (unsigned long)app->cvm_amount_y);
                canvas_draw_str(canvas, 2, 20, line);
                int y = 30;
                size_t shown = app->cvm_rules_count > 4 ? 4 : app->cvm_rules_count;
                for(size_t i = 0; i < shown; i++) {
                    snprintf(line, sizeof(line), "%s %s%s",
                        emv_cvm_method_label(app->cvm_rules[i].method),
                        emv_cvm_condition_label(app->cvm_rules[i].condition),
                        app->cvm_rules[i].fail_continues ? " >>" : "");
                    canvas_draw_str(canvas, 2, y, line);
                    y += 8;
                }
            }
            canvas_draw_str(canvas, 2, 63, "L/R pages");
            break;
        }
        case PageAids: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 10, "AIDs found");
            canvas_set_font(canvas, FontSecondary);
            if(app->aids_count == 0) {
                canvas_draw_str(canvas, 2, 22, "(none)");
            } else {
                int y = 20;
                int max_show = 4;
                int start = app->aids_scroll;
                if(start < 0) start = 0;
                if(start >= (int)app->aids_count) start = app->aids_count - 1;
                int end = start + max_show;
                if(end > (int)app->aids_count) end = app->aids_count;
                for(int i = start; i < end; i++) {
                    int n = snprintf(line, sizeof(line), "%s ",
                        emv_aid_label(app->aids[i].aid, app->aids[i].aid_len));
                    for(uint8_t b = 0; b < app->aids[i].aid_len && n < (int)sizeof(line) - 3; b++) {
                        n += snprintf(line + n, sizeof(line) - n, "%02X", app->aids[i].aid[b]);
                    }
                    canvas_draw_str(canvas, 2, y, line);
                    y += 10;
                }
                if((int)app->aids_count > max_show) {
                    canvas_draw_str(canvas, 2, 60, "U/D scroll");
                }
            }
            canvas_draw_str(canvas, 70, 63, "L/R pages");
            break;
        }
        case PageHex: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 2, 10, "Records hex");
            canvas_set_font(canvas, FontSecondary);
            size_t total = app->records_len;
            size_t per_line = 16;
            int max_lines = 5;
            int line_offset = app->hex_scroll;
            if(line_offset < 0) line_offset = 0;
            for(int row = 0; row < max_lines; row++) {
                size_t off = (size_t)(line_offset + row) * per_line;
                if(off >= total) break;
                int n = 0;
                n += snprintf(line + n, sizeof(line) - n, "%03X ", (unsigned)off);
                for(size_t b = 0; b < per_line && off + b < total; b++) {
                    n += snprintf(line + n, sizeof(line) - n, "%02X", app->records[off + b]);
                }
                canvas_draw_str(canvas, 0, 20 + row * 9, line);
            }
            canvas_draw_str(canvas, 2, 64, "U/D scroll  L/R pages");
            break;
        }
        default: break;
        }
        break;
    }

    case UiStateError:
        canvas_draw_str(canvas, 2, 12, "Error");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 28, app->err_msg[0] ? app->err_msg : "Unknown error");
        canvas_draw_str(canvas, 2, 56, "OK = Retry  BACK = Exit");
        break;
    }

    furi_mutex_release(app->state_mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    EmvReaderApp* app = ctx;
    furi_message_queue_put(app->input_queue, event, FuriWaitForever);
}

static bool emv_send_apdu(
    Iso14443_4aPoller* poller,
    BitBuffer* tx,
    BitBuffer* rx,
    const uint8_t* cmd,
    size_t cmd_len) {
    bit_buffer_reset(tx);
    bit_buffer_append_bytes(tx, cmd, cmd_len);
    Iso14443_4aError err = iso14443_4a_poller_send_block(poller, tx, rx);
    return err == Iso14443_4aErrorNone;
}

typedef struct {
    AidEntry* entries;
    uint8_t max;
    uint8_t count;
} AidCollectCtx;

static bool aid_collect_cb(const BerTlvField* field, void* ctx) {
    AidCollectCtx* c = ctx;
    if(field->tag == EMV_TAG_AID && field->length > 0 && field->length <= 16 && c->count < c->max) {
        memcpy(c->entries[c->count].aid, field->value, field->length);
        c->entries[c->count].aid_len = field->length;
        c->count++;
    }
    return true;
}

static bool extract_aids_from_ppse(
    const uint8_t* resp,
    size_t len,
    AidEntry* entries,
    uint8_t max,
    uint8_t* out_count) {
    if(len < 2) return false;
    size_t body = len - 2;
    AidCollectCtx ctx = {.entries = entries, .max = max, .count = 0};
    ber_tlv_walk(resp, body, aid_collect_cb, &ctx);
    *out_count = ctx.count;
    return ctx.count > 0;
}

static void parse_card_data(EmvReaderApp* app) {
    const uint8_t* v = NULL;
    size_t l = 0;

    if(ber_tlv_find(app->records, app->records_len, EMV_TAG_PAN, &v, &l)) {
        emv_decode_pan_bcd(v, l, app->pan_str, sizeof(app->pan_str));
    }
    if(ber_tlv_find(app->records, app->records_len, EMV_TAG_EXPIRY, &v, &l)) {
        emv_decode_expiry_bcd(v, l, app->expiry_str, sizeof(app->expiry_str));
    }
    if(ber_tlv_find(app->records, app->records_len, EMV_TAG_HOLDER_NAME, &v, &l)) {
        size_t copy = l < sizeof(app->holder_str) - 1 ? l : sizeof(app->holder_str) - 1;
        memcpy(app->holder_str, v, copy);
        app->holder_str[copy] = '\0';
    }
    if(ber_tlv_find(app->records, app->records_len, EMV_TAG_TRACK2_EQUIV, &v, &l)) {
        emv_decode_track2(v, l, app->track2_str, sizeof(app->track2_str));
    }

    if(app->pan_str[0] == '\0' && app->track2_str[0] != '\0') {
        size_t i = 0;
        while(i < sizeof(app->pan_str) - 1 && app->track2_str[i] && app->track2_str[i] != '=') {
            app->pan_str[i] = app->track2_str[i];
            i++;
        }
        app->pan_str[i] = '\0';
        if(app->expiry_str[0] == '\0' && app->track2_str[i] == '=' &&
           app->track2_str[i + 1] && app->track2_str[i + 2] &&
           app->track2_str[i + 3] && app->track2_str[i + 4]) {
            app->expiry_str[0] = app->track2_str[i + 3];
            app->expiry_str[1] = app->track2_str[i + 4];
            app->expiry_str[2] = '/';
            app->expiry_str[3] = app->track2_str[i + 1];
            app->expiry_str[4] = app->track2_str[i + 2];
            app->expiry_str[5] = '\0';
        }
    }

    if(!app->aip_present) {
        if(ber_tlv_find(app->records, app->records_len, EMV_TAG_AIP, &v, &l) && l == 2) {
            app->aip[0] = v[0];
            app->aip[1] = v[1];
            app->aip_present = true;
        }
    }
    if(app->aip_present) {
        emv_decode_aip(app->aip, app->aip_decoded, sizeof(app->aip_decoded));
    }

    if(ber_tlv_find(app->records, app->records_len, EMV_TAG_ATC, &v, &l) && l == 2) {
        app->atc[0] = v[0];
        app->atc[1] = v[1];
        app->atc_present = true;
    }

    const uint8_t* cvm_v = NULL;
    size_t cvm_l = 0;
    if(ber_tlv_find(app->records, app->records_len, 0x8E, &cvm_v, &cvm_l)) {
        size_t copy = cvm_l < sizeof(app->cvm_list_buf) ? cvm_l : sizeof(app->cvm_list_buf);
        memcpy(app->cvm_list_buf, cvm_v, copy);
        app->cvm_list_len = copy;
        emv_parse_cvm_list(
            app->cvm_list_buf, app->cvm_list_len,
            app->cvm_rules, MAX_CVM_RULES, &app->cvm_rules_count,
            &app->cvm_amount_x, &app->cvm_amount_y);
    }

    if(app->track2_str[0]) {
        if(emv_extract_service_code(app->track2_str, app->service_code, sizeof(app->service_code))) {
            emv_service_code_describe(app->service_code, app->service_code_desc, sizeof(app->service_code_desc));
        }
    }

    app->pin_analysis = emv_analyze_pin(
        app->aip_present, app->aip,
        app->cvm_rules, app->cvm_rules_count);
    emv_format_pin_status(&app->pin_analysis, app->pin_status_str, sizeof(app->pin_status_str));

    const char* label = emv_aid_label(app->aid, app->aid_len);
    snprintf(app->card_label, sizeof(app->card_label), "%s", label);
}

typedef struct {
    uint32_t tag;
    const uint8_t* value;
    size_t len;
} PdolDefault;

static const uint8_t pdol_ttq[]       = {0x36, 0xA0, 0x40, 0x00};
static const uint8_t pdol_amount[]    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
static const uint8_t pdol_amt_other[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t pdol_country[]   = {0x08, 0x40};
static const uint8_t pdol_tvr[]       = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t pdol_currency[]  = {0x08, 0x40};
static const uint8_t pdol_txn_date[]  = {0x26, 0x05, 0x08};
static const uint8_t pdol_txn_type[]  = {0x00};
static const uint8_t pdol_un[]        = {0x01, 0x02, 0x03, 0x04};
static const uint8_t pdol_term_type[] = {0x22};
static const uint8_t pdol_term_caps[] = {0x60, 0x00, 0xF0, 0xA0, 0x01};
static const uint8_t pdol_app_ver[]   = {0x00, 0x8C};

static const PdolDefault pdol_defaults[] = {
    {0x9F66, pdol_ttq,       sizeof(pdol_ttq)},
    {0x9F02, pdol_amount,    sizeof(pdol_amount)},
    {0x9F03, pdol_amt_other, sizeof(pdol_amt_other)},
    {0x9F1A, pdol_country,   sizeof(pdol_country)},
    {0x95,   pdol_tvr,       sizeof(pdol_tvr)},
    {0x5F2A, pdol_currency,  sizeof(pdol_currency)},
    {0x9A,   pdol_txn_date,  sizeof(pdol_txn_date)},
    {0x9C,   pdol_txn_type,  sizeof(pdol_txn_type)},
    {0x9F37, pdol_un,        sizeof(pdol_un)},
    {0x9F35, pdol_term_type, sizeof(pdol_term_type)},
    {0x9F40, pdol_term_caps, sizeof(pdol_term_caps)},
    {0x9F09, pdol_app_ver,   sizeof(pdol_app_ver)},
};

static size_t build_pdol_data(
    const uint8_t* pdol_req,
    size_t pdol_req_len,
    uint8_t* out,
    size_t out_size) {
    size_t out_pos = 0;
    size_t pos = 0;
    while(pos < pdol_req_len) {
        uint32_t tag = 0;
        size_t tag_len = ber_tlv_read_tag(pdol_req + pos, pdol_req_len - pos, &tag);
        if(tag_len == 0) break;
        pos += tag_len;
        if(pos >= pdol_req_len) break;
        size_t req_len = 0;
        size_t len_len = ber_tlv_read_length(pdol_req + pos, pdol_req_len - pos, &req_len);
        if(len_len == 0) break;
        pos += len_len;

        if(out_pos + req_len > out_size) break;

        const uint8_t* def_val = NULL;
        size_t def_len = 0;
        for(size_t i = 0; i < sizeof(pdol_defaults) / sizeof(pdol_defaults[0]); i++) {
            if(pdol_defaults[i].tag == tag) {
                def_val = pdol_defaults[i].value;
                def_len = pdol_defaults[i].len;
                break;
            }
        }

        if(def_val && def_len >= req_len) {
            memcpy(out + out_pos, def_val, req_len);
        } else if(def_val) {
            memcpy(out + out_pos, def_val, def_len);
            memset(out + out_pos + def_len, 0, req_len - def_len);
        } else {
            memset(out + out_pos, 0, req_len);
        }
        out_pos += req_len;
    }
    return out_pos;
}

static NfcCommand emv_poller_cb(NfcGenericEvent event, void* context) {
    EmvReaderApp* app = context;
    const Iso14443_4aPollerEvent* iso_event = event.event_data;
    if(iso_event->type != Iso14443_4aPollerEventTypeReady) {
        return NfcCommandContinue;
    }
    if(app->poll_done) return NfcCommandStop;

    Iso14443_4aPoller* poller = event.instance;
    BitBuffer* tx = bit_buffer_alloc(APDU_BUF_SIZE);
    BitBuffer* rx = bit_buffer_alloc(APDU_BUF_SIZE);

    uint8_t apdu[APDU_BUF_SIZE];
    size_t apdu_len;

    apdu_len = emv_apdu_build_select_ppse(apdu, sizeof(apdu));
    if(!emv_send_apdu(poller, tx, rx, apdu, apdu_len)) {
        snprintf(app->err_msg, sizeof(app->err_msg), "SELECT PPSE failed");
        goto fail;
    }
    {
        const uint8_t* resp = bit_buffer_get_data(rx);
        size_t resp_len = bit_buffer_get_size_bytes(rx);
        if(!extract_aids_from_ppse(resp, resp_len, app->aids, MAX_AIDS, &app->aids_count)) {
            snprintf(app->err_msg, sizeof(app->err_msg), "No AID in PPSE");
            goto fail;
        }
        memcpy(app->aid, app->aids[0].aid, app->aids[0].aid_len);
        app->aid_len = app->aids[0].aid_len;
    }

    apdu_len = emv_apdu_build_select_aid(app->aid, app->aid_len, apdu, sizeof(apdu));
    if(!emv_send_apdu(poller, tx, rx, apdu, apdu_len)) {
        snprintf(app->err_msg, sizeof(app->err_msg), "SELECT AID failed");
        goto fail;
    }

    uint8_t pdol_data[252];
    size_t pdol_data_len = 0;
    {
        const uint8_t* fci = bit_buffer_get_data(rx);
        size_t fci_len = bit_buffer_get_size_bytes(rx);
        if(fci_len >= 2) {
            size_t fci_body = fci_len - 2;
            const uint8_t* pdol_req = NULL;
            size_t pdol_req_len = 0;
            if(ber_tlv_find(fci, fci_body, EMV_TAG_PDOL, &pdol_req, &pdol_req_len) &&
               pdol_req_len > 0) {
                pdol_data_len = build_pdol_data(
                    pdol_req, pdol_req_len, pdol_data, sizeof(pdol_data));
            }
        }
    }

    apdu_len = emv_apdu_build_gpo(pdol_data, pdol_data_len, apdu, sizeof(apdu));
    if(!emv_send_apdu(poller, tx, rx, apdu, apdu_len)) {
        snprintf(app->err_msg, sizeof(app->err_msg), "GPO failed");
        goto fail;
    }
    app->records_len = 0;
    {
        const uint8_t* resp = bit_buffer_get_data(rx);
        size_t resp_len = bit_buffer_get_size_bytes(rx);
        if(resp_len < 4) {
            uint8_t s1 = resp_len >= 2 ? resp[resp_len - 2] : 0;
            uint8_t s2 = resp_len >= 1 ? resp[resp_len - 1] : 0;
            snprintf(app->err_msg, sizeof(app->err_msg),
                "GPO SW=%02X%02X pdol=%u", s1, s2, (unsigned)pdol_data_len);
            goto fail;
        }
        size_t body = resp_len - 2;

        if(body > 0 && body <= sizeof(app->records)) {
            memcpy(app->records, resp, body);
            app->records_len = body;
        }

        const uint8_t* aip_v = NULL;
        size_t aip_l = 0;
        const uint8_t* afl_v = NULL;
        size_t afl_l = 0;
        if(ber_tlv_find(resp, body, EMV_TAG_AIP, &aip_v, &aip_l) && aip_l == 2) {
            app->aip[0] = aip_v[0];
            app->aip[1] = aip_v[1];
            app->aip_present = true;
        }
        if(ber_tlv_find(resp, body, EMV_TAG_AFL, &afl_v, &afl_l)) {
            if(afl_l <= sizeof(app->afl)) {
                memcpy(app->afl, afl_v, afl_l);
                app->afl_len = afl_l;
            }
        } else if(resp[0] == EMV_TAG_RESPONSE_TPL1 && resp[1] >= 2 && body >= (size_t)resp[1] + 2) {
            size_t inner_len = resp[1];
            if(inner_len >= 2) {
                app->aip[0] = resp[2];
                app->aip[1] = resp[3];
                app->aip_present = true;
                size_t afl_inner = inner_len - 2;
                if(afl_inner <= sizeof(app->afl)) {
                    memcpy(app->afl, resp + 4, afl_inner);
                    app->afl_len = afl_inner;
                }
            }
        }
    }

    for(size_t i = 0; i + 3 < app->afl_len; i += 4) {
        uint8_t sfi = app->afl[i] >> 3;
        uint8_t first = app->afl[i + 1];
        uint8_t last = app->afl[i + 2];
        if(sfi == 0 || first == 0 || last < first) continue;
        for(uint8_t r = first; r <= last; r++) {
            apdu_len = emv_apdu_build_read_record(sfi, r, apdu, sizeof(apdu));
            if(!emv_send_apdu(poller, tx, rx, apdu, apdu_len)) continue;
            const uint8_t* resp = bit_buffer_get_data(rx);
            size_t resp_len = bit_buffer_get_size_bytes(rx);
            if(resp_len < 3) continue;
            if(resp[resp_len - 2] != 0x90) continue;
            size_t body = resp_len - 2;
            if(app->records_len + body > sizeof(app->records)) break;
            memcpy(app->records + app->records_len, resp, body);
            app->records_len += body;
        }
    }

    if(app->records_len == 0) {
        snprintf(app->err_msg, sizeof(app->err_msg), "No data");
        goto fail;
    }

    parse_card_data(app);

    if(app->pan_str[0] == '\0' && app->track2_str[0] == '\0') {
        snprintf(app->err_msg, sizeof(app->err_msg),
            "No PAN; afl=%u rec=%u", (unsigned)app->afl_len, (unsigned)app->records_len);
        goto fail;
    }

    app->poll_success = true;
    app->poll_done = true;
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    furi_event_flag_set(app->result_flag, 1);
    return NfcCommandStop;

fail:
    app->poll_success = false;
    app->poll_done = true;
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    furi_event_flag_set(app->result_flag, 1);
    return NfcCommandStop;
}

static void save_session(EmvReaderApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SAVE_DIR);

    const char* last4 = "xxxx";
    size_t pan_len = strlen(app->pan_str);
    if(pan_len >= 4) last4 = app->pan_str + pan_len - 4;

    uint32_t ts = furi_hal_rtc_get_timestamp();
    char path[128];
    snprintf(path, sizeof(path), SAVE_DIR "/%lu_%s.txt", (unsigned long)ts, last4);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        int n;
        n = snprintf(buf, sizeof(buf), "EMV Reader dump\n");
        storage_file_write(file, buf, n);
        n = snprintf(buf, sizeof(buf), "Card: %s\nPAN: %s\nExp: %s\nHolder: %s\nTrack2: %s\n",
            app->card_label, app->pan_str, app->expiry_str, app->holder_str, app->track2_str);
        storage_file_write(file, buf, n);
        if(app->service_code[0]) {
            n = snprintf(buf, sizeof(buf), "ServiceCode: %s (%s)\n",
                app->service_code, app->service_code_desc);
            storage_file_write(file, buf, n);
        }
        if(app->aip_present) {
            n = snprintf(buf, sizeof(buf), "AIP: %02X%02X (%s)\n",
                app->aip[0], app->aip[1], app->aip_decoded);
            storage_file_write(file, buf, n);
        }
        if(app->atc_present) {
            uint16_t atc = ((uint16_t)app->atc[0] << 8) | app->atc[1];
            n = snprintf(buf, sizeof(buf), "ATC: %u\n", atc);
            storage_file_write(file, buf, n);
        }
        if(app->pin_status_str[0]) {
            n = snprintf(buf, sizeof(buf), "PIN: %s\n", app->pin_status_str);
            storage_file_write(file, buf, n);
        }

        n = snprintf(buf, sizeof(buf), "\nAIDs (%u):\n", app->aids_count);
        storage_file_write(file, buf, n);
        for(uint8_t i = 0; i < app->aids_count; i++) {
            n = snprintf(buf, sizeof(buf), "  %s ",
                emv_aid_label(app->aids[i].aid, app->aids[i].aid_len));
            storage_file_write(file, buf, n);
            for(uint8_t b = 0; b < app->aids[i].aid_len; b++) {
                n = snprintf(buf, sizeof(buf), "%02X", app->aids[i].aid[b]);
                storage_file_write(file, buf, n);
            }
            storage_file_write(file, "\n", 1);
        }

        if(app->cvm_rules_count > 0) {
            n = snprintf(buf, sizeof(buf), "\nCVM list (X=%lu Y=%lu):\n",
                (unsigned long)app->cvm_amount_x, (unsigned long)app->cvm_amount_y);
            storage_file_write(file, buf, n);
            for(size_t i = 0; i < app->cvm_rules_count; i++) {
                n = snprintf(buf, sizeof(buf), "  %s if %s%s\n",
                    emv_cvm_method_label(app->cvm_rules[i].method),
                    emv_cvm_condition_label(app->cvm_rules[i].condition),
                    app->cvm_rules[i].fail_continues ? " (fail->next)" : "");
                storage_file_write(file, buf, n);
            }
        }

        n = snprintf(buf, sizeof(buf), "\nRecords (%u bytes):\n", (unsigned)app->records_len);
        storage_file_write(file, buf, n);
        for(size_t i = 0; i < app->records_len; i++) {
            n = snprintf(buf, sizeof(buf), "%02X", app->records[i]);
            storage_file_write(file, buf, n);
            if((i & 0x0F) == 0x0F) storage_file_write(file, "\n", 1);
        }
        storage_file_write(file, "\n", 1);
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void start_scan(EmvReaderApp* app) {
    emv_reader_reset_card(app);
    app->ui_state = UiStateScanning;
    view_port_update(app->view_port);

    furi_event_flag_clear(app->result_flag, 1);

    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_4a);
    nfc_poller_start(app->poller, emv_poller_cb, app);
}

static void stop_scan(EmvReaderApp* app) {
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

int32_t emv_reader_app(void* p) {
    UNUSED(p);
    EmvReaderApp* app = malloc(sizeof(EmvReaderApp));
    memset(app, 0, sizeof(EmvReaderApp));

    app->state_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->result_flag = furi_event_flag_alloc();

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    app->nfc = nfc_alloc();
    app->ui_state = UiStateIdle;

    InputEvent input;
    while(!app->exit_requested) {
        FuriStatus rc = furi_message_queue_get(app->input_queue, &input, 100);

        if(app->ui_state == UiStateScanning && app->poller) {
            uint32_t flags = furi_event_flag_get(app->result_flag);
            if(flags & 1) {
                furi_event_flag_clear(app->result_flag, 1);
                stop_scan(app);
                furi_mutex_acquire(app->state_mutex, FuriWaitForever);
                if(app->poll_success) {
                    app->ui_state = UiStateDone;
                    save_session(app);
                } else {
                    if(app->err_msg[0] == '\0') {
                        snprintf(app->err_msg, sizeof(app->err_msg), "No card / unknown");
                    }
                    app->ui_state = UiStateError;
                }
                furi_mutex_release(app->state_mutex);
                view_port_update(app->view_port);
            }
        }

        if(rc != FuriStatusOk) continue;
        if(input.type != InputTypeShort && input.type != InputTypeRepeat) continue;

        furi_mutex_acquire(app->state_mutex, FuriWaitForever);
        switch(app->ui_state) {
        case UiStateIdle:
            if(input.key == InputKeyOk) {
                furi_mutex_release(app->state_mutex);
                start_scan(app);
                continue;
            } else if(input.key == InputKeyBack) {
                app->exit_requested = true;
            }
            break;

        case UiStateScanning:
            if(input.key == InputKeyBack) {
                furi_mutex_release(app->state_mutex);
                stop_scan(app);
                furi_mutex_acquire(app->state_mutex, FuriWaitForever);
                app->ui_state = UiStateIdle;
            }
            break;

        case UiStateDone:
            if(input.key == InputKeyOk) {
                furi_mutex_release(app->state_mutex);
                start_scan(app);
                continue;
            } else if(input.key == InputKeyRight) {
                app->page = (app->page + 1) % PageCount;
                app->hex_scroll = 0;
                app->aids_scroll = 0;
            } else if(input.key == InputKeyLeft) {
                app->page = (app->page + PageCount - 1) % PageCount;
                app->hex_scroll = 0;
                app->aids_scroll = 0;
            } else if(input.key == InputKeyDown) {
                if(app->page == PageHex) app->hex_scroll++;
                else if(app->page == PageAids && app->aids_scroll < (int)app->aids_count - 1) app->aids_scroll++;
            } else if(input.key == InputKeyUp) {
                if(app->page == PageHex && app->hex_scroll > 0) app->hex_scroll--;
                else if(app->page == PageAids && app->aids_scroll > 0) app->aids_scroll--;
            } else if(input.key == InputKeyBack) {
                app->exit_requested = true;
            }
            break;

        case UiStateError:
            if(input.key == InputKeyOk) {
                furi_mutex_release(app->state_mutex);
                start_scan(app);
                continue;
            } else if(input.key == InputKeyBack) {
                app->exit_requested = true;
            }
            break;
        }
        furi_mutex_release(app->state_mutex);
        view_port_update(app->view_port);
    }

    stop_scan(app);

    nfc_free(app->nfc);
    gui_remove_view_port(app->gui, app->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(app->view_port);
    furi_message_queue_free(app->input_queue);
    furi_event_flag_free(app->result_flag);
    furi_mutex_free(app->state_mutex);
    free(app);
    return 0;
}
