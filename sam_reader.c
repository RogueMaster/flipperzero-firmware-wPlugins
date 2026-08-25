#include "sam_reader.h"

#include "seader_i.h"
#include "seader_worker_i.h"
#include "t_1.h"
#include "usb_ccid_reader.h"
#include "trace_log.h"

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

static bool seader_reader_relay_once(
    Seader* seader,
    SeaderReader* reader,
    const uint8_t* apdu,
    uint16_t apdu_len,
    uint8_t* out,
    uint16_t out_cap,
    uint16_t* out_len,
    uint32_t timeout_ms);

#define TAG "SeaderReader"

#define SEADER_READER_CFG_PATH STORAGE_APP_DATA_PATH_PREFIX "/usb_reader.conf"
static const char* seader_reader_cfg_header = "Seader USB Reader";
static const uint32_t seader_reader_cfg_version = 1;

/* Grace SAM ATR used only if the live ATR was not captured. */
static const uint8_t SEADER_READER_FALLBACK_ATR[] = {
    0x3b, 0x95, 0x96, 0x80, 0xb1, 0xfe, 0x55, 0x1f, 0xc7, 0x47, 0x72, 0x61, 0x63, 0x65, 0x13};

struct SeaderReader {
    SeaderReaderConfig cfg;

    FuriMutex* lock;
    FuriSemaphore* resp_ready;

    bool waiting;
    uint32_t resp_len;
    uint8_t resp_buf[SEADER_READER_MAX_APDU];
    uint8_t tx_buf[SEADER_READER_MAX_APDU];

    uint32_t apdu_count;
};

void seader_reader_config_default(SeaderReaderConfig* cfg) {
    furi_check(cfg);
    memset(cfg, 0, sizeof(*cfg));
    cfg->vid = SEADER_READER_DEFAULT_VID;
    cfg->pid = SEADER_READER_DEFAULT_PID;
    strncpy(cfg->manufacturer, SEADER_READER_DEFAULT_MANUF, SEADER_READER_NAME_MAX - 1);
    strncpy(cfg->product, SEADER_READER_DEFAULT_PRODUCT, SEADER_READER_NAME_MAX - 1);
}

/* -------------------- Persisted identity (name + PID) -------------------- */

void seader_reader_settings_load(Seader* seader) {
    furi_check(seader);
    /* Defaults first. */
    strlcpy(seader->reader_manufacturer, SEADER_READER_DEFAULT_MANUF, SEADER_READER_NAME_MAX);
    strlcpy(seader->reader_product, SEADER_READER_DEFAULT_PRODUCT, SEADER_READER_NAME_MAX);
    seader->reader_pid = SEADER_READER_DEFAULT_PID;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    uint32_t version = 0;
    do {
        if(!flipper_format_file_open_existing(file, SEADER_READER_CFG_PATH)) break;
        if(!flipper_format_read_header(file, tmp, &version)) break;
        if(furi_string_cmp_str(tmp, seader_reader_cfg_header) ||
           version != seader_reader_cfg_version)
            break;
        if(flipper_format_read_string(file, "Manufacturer", tmp)) {
            strlcpy(seader->reader_manufacturer, furi_string_get_cstr(tmp), SEADER_READER_NAME_MAX);
        }
        if(flipper_format_read_string(file, "Product", tmp)) {
            strlcpy(seader->reader_product, furi_string_get_cstr(tmp), SEADER_READER_NAME_MAX);
        }
        uint32_t pid = 0;
        if(flipper_format_read_uint32(file, "PID", &pid, 1) && pid != 0 && pid <= 0xFFFF) {
            seader->reader_pid = (uint16_t)pid;
        }
    } while(false);
    furi_string_free(tmp);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

void seader_reader_settings_save(Seader* seader) {
    furi_check(seader);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    do {
        if(!flipper_format_file_open_always(file, SEADER_READER_CFG_PATH)) break;
        if(!flipper_format_write_header_cstr(
               file, seader_reader_cfg_header, seader_reader_cfg_version))
            break;
        if(!flipper_format_write_string_cstr(
               file, "Manufacturer", seader->reader_manufacturer))
            break;
        if(!flipper_format_write_string_cstr(file, "Product", seader->reader_product)) break;
        uint32_t pid = seader->reader_pid;
        if(!flipper_format_write_uint32(file, "PID", &pid, 1)) break;
    } while(false);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* -------------------- CCID gadget callbacks -------------------- */

/* Called from the USB gadget on ICC power-on (USB thread). */
/* Grace getSamVersion single-shot, used as a sacrificial warm-up. */
static const uint8_t SEADER_READER_WARMUP[] = {0xA0, 0xDA, 0x02, 0x63, 0x00, 0x00, 0x0A, 0x44, 0x0A,
                                               0x44, 0x00, 0x00, 0x00, 0xA0, 0x02, 0x82, 0x00, 0x00,
                                               0x00};

static void seader_reader_get_atr(void* ctx, uint8_t* atr, uint16_t* atr_len) {
    Seader* seader = ctx;
    SeaderReader* reader = seader->reader;

    /* Warm up the SAM link on power-on: the first exchange after connect can be
       dropped/rejected (SamKeyScan resets+retries for the same reason), so send
       a sacrificial getSamVersion and discard the result before the host's real
       commands arrive. */
    if(reader) {
        uint8_t scratch[64];
        uint16_t slen = 0;
        seader_reader_relay_once(
            seader,
            reader,
            SEADER_READER_WARMUP,
            sizeof(SEADER_READER_WARMUP),
            scratch,
            sizeof(scratch),
            &slen,
            1000); /* short timeout so a mute SAM doesn't stall SCardConnect */
    }

    if(seader->ATR_len > 0 && seader->ATR_len <= SEADER_MAX_ATR_SIZE) {
        memcpy(atr, seader->ATR, seader->ATR_len);
        *atr_len = (uint16_t)seader->ATR_len;
    } else {
        memcpy(atr, SEADER_READER_FALLBACK_ATR, sizeof(SEADER_READER_FALLBACK_ATR));
        *atr_len = (uint16_t)sizeof(SEADER_READER_FALLBACK_ATR);
    }
    FURI_LOG_I(TAG, "ICC power on, ATR len=%u", (unsigned)*atr_len);
}

/* Undo WUDF's corruption of the extended Grace single-shot: on some Windows
   builds WUDF changes the class byte (A0->00) and re-encodes the extended
   length as short, so the SAM's Grace PUT DATA (INS=DA, P1P2=0263) fails (6E00
   for the class byte, 6700 for the short length). We restore CLA=A0 and rebuild
   the extended encoding the SAM expects, writing the result into dst and
   returning its new length. Everything else is copied verbatim: non-DA
   commands, already-extended commands, and the SnmpLoader forms (P1=80 /
   P1P2=0000) whose short encoding the SAM accepts. */
static uint16_t seader_reader_fix_command(uint8_t* dst, const uint8_t* apdu, uint16_t apdu_len) {
    memcpy(dst, apdu, apdu_len);
    if(apdu_len < 5 || dst[1] != 0xDA) return apdu_len;

    if(dst[0] == 0x00) dst[0] = 0xA0; /* restore class byte */

    if(dst[4] == 0x00) return apdu_len; /* already extended */
    /* Only the Grace single-shot (P1P2=0263) needs the extended encoding
       rebuilt. SnmpLoader (P1=80 / P1P2=0000) works in its short form, and
       reconstructing it would desync SamKeyScan's chunked read fallback. */
    if(!(dst[2] == 0x02 && dst[3] == 0x63)) return apdu_len;

    uint8_t lc = dst[4];
    if((uint16_t)(5 + lc) > apdu_len) return apdu_len; /* malformed short APDU */
    bool case4 = apdu_len > (uint16_t)(5 + lc);
    uint16_t new_len = (uint16_t)(7 + lc + (case4 ? 2 : 0));
    if(new_len > SEADER_READER_MAX_APDU) return apdu_len; /* no room */

    /* CLA INS P1 P2 | 00 <Lc hi> <Lc lo> | data | 00 00 (extended Le, case 4). */
    memmove(dst + 7, dst + 5, lc);
    dst[4] = 0x00;
    dst[5] = 0x00;
    dst[6] = lc;
    if(case4) {
        dst[7 + lc] = 0x00;
        dst[7 + lc + 1] = 0x00;
    }
    return new_len;
}

/* Relay one APDU to the SAM and block for its response. Copies up to out_cap
   bytes into out and sets *out_len. Runs on the ccid worker thread; the UART-RX
   thread wakes us via seader_reader_sam_response. Returns true on success. */
static bool seader_reader_relay_once(
    Seader* seader,
    SeaderReader* reader,
    const uint8_t* apdu,
    uint16_t apdu_len,
    uint8_t* out,
    uint16_t out_cap,
    uint16_t* out_len,
    uint32_t timeout_ms) {
    if(apdu_len == 0 || apdu_len > SEADER_READER_MAX_APDU) return false;

    furi_semaphore_acquire(reader->resp_ready, 0); /* drain stale token */
    furi_mutex_acquire(reader->lock, FuriWaitForever);
    reader->resp_len = 0;
    reader->waiting = true;
    uint16_t tx_len = seader_reader_fix_command(reader->tx_buf, apdu, apdu_len);
    furi_mutex_release(reader->lock);

    seader_trace_hex("Reader", "SAM TX", reader->tx_buf, tx_len);

    SeaderUartBridge* uart = seader->uart;
    if(uart->T == 1) {
        seader_send_t1(uart, reader->tx_buf, tx_len);
    } else {
        seader_ccid_XfrBlock(uart, reader->tx_buf, tx_len);
    }

    FuriStatus st = furi_semaphore_acquire(reader->resp_ready, furi_ms_to_ticks(timeout_ms));

    bool ok = false;
    furi_mutex_acquire(reader->lock, FuriWaitForever);
    reader->waiting = false;
    if(st == FuriStatusOk && reader->resp_len >= 2) {
        uint16_t n = (uint16_t)reader->resp_len;
        seader_trace_hex("Reader", "SAM RX", reader->resp_buf, reader->resp_len);
        if(n > out_cap) n = out_cap;
        memcpy(out, reader->resp_buf, n);
        *out_len = n;
        ok = true;
    } else {
        seader_trace("Reader", "SAM RX none/short st=%d len=%lu", st, (unsigned long)reader->resp_len);
    }
    furi_mutex_release(reader->lock);
    return ok;
}

/* Called from the CCID worker thread for each host APDU. Relays to the SAM and,
   like a real ACR39U at short-APDU level, handles T=0 GET RESPONSE (SW1=0x61)
   reader-side so the host sees one complete response. Always fills resp. */
static bool seader_reader_xfr(
    void* ctx,
    const uint8_t* apdu,
    uint16_t apdu_len,
    uint8_t* resp,
    uint16_t* resp_len) {
    Seader* seader = ctx;
    SeaderReader* reader = seader->reader;
    if(!reader) {
        *resp_len = 0;
        return false;
    }

    if(apdu_len == 0 || apdu_len > SEADER_READER_MAX_APDU) {
        FURI_LOG_W(TAG, "Rejecting APDU len=%u", (unsigned)apdu_len);
        resp[0] = 0x6F;
        resp[1] = 0x00;
        *resp_len = 2;
        return true;
    }

    uint16_t total = 0;
    if(!seader_reader_relay_once(
           seader, reader, apdu, apdu_len, resp, SEADER_CCID_MAX_RESP, &total,
           SEADER_READER_TIMEOUT_MS)) {
        FURI_LOG_W(TAG, "SAM relay timeout/short");
        resp[0] = 0x6F;
        resp[1] = 0x00;
        *resp_len = 2;
        reader->apdu_count++;
        return true;
    }

    /* Reader-side GET RESPONSE loop for SW1=0x61. */
    uint8_t guard = 0;
    while(total >= 2 && resp[total - 2] == 0x61 && guard++ < 16) {
        uint8_t le = resp[total - 1];
        total -= 2; /* strip the 61xx SW; keep any leading data */
        uint16_t cap = (total < SEADER_CCID_MAX_RESP) ? (uint16_t)(SEADER_CCID_MAX_RESP - total) : 0;
        if(cap < 2) break;
        uint8_t get_response[5] = {0x00, 0xC0, 0x00, 0x00, le};
        uint16_t got = 0;
        if(!seader_reader_relay_once(
               seader, reader, get_response, 5, resp + total, cap, &got, SEADER_READER_TIMEOUT_MS)) {
            break;
        }
        total += got;
    }

    *resp_len = total;
    reader->apdu_count++;
    return true;
}

/* -------------------- SAM response (UART-RX thread context) --------------- */

bool seader_reader_sam_response(Seader* seader, uint8_t* apdu, uint32_t len) {
    furi_check(seader);
    SeaderReader* reader = seader->reader;
    if(!reader) {
        return false;
    }

    furi_mutex_acquire(reader->lock, FuriWaitForever);
    if(reader->waiting) {
        uint32_t n = len;
        if(n > sizeof(reader->resp_buf)) {
            n = sizeof(reader->resp_buf);
        }
        memcpy(reader->resp_buf, apdu, n);
        reader->resp_len = n;
        furi_semaphore_release(reader->resp_ready);
    } else {
        FURI_LOG_D(TAG, "Dropping unsolicited SAM response len=%lu", (unsigned long)len);
    }
    furi_mutex_release(reader->lock);
    return true;
}

uint32_t seader_reader_apdu_count(Seader* seader) {
    if(!seader || !seader->reader) {
        return 0;
    }
    return seader->reader->apdu_count;
}

const char* seader_reader_active_name(Seader* seader) {
    if(!seader || !seader->reader) {
        return SEADER_READER_DEFAULT_PRODUCT;
    }
    return seader->reader->cfg.product;
}

uint32_t seader_reader_ccid_count(void) {
    uint32_t count = 0;
    seader_usb_ccid_reader_stats(NULL, &count);
    return count;
}

const char* seader_reader_ccid_last_name(void) {
    uint8_t last = 0;
    seader_usb_ccid_reader_stats(&last, NULL);
    return seader_usb_ccid_cmd_name(last);
}

void seader_reader_ccid_debug(uint16_t* atr_len, int32_t* tx_last) {
    seader_usb_ccid_reader_debug(atr_len, tx_last);
}

/* -------------------- Lifecycle (worker thread) --------------------------- */

static SeaderReader* seader_reader_alloc(void) {
    SeaderReader* reader = malloc(sizeof(SeaderReader));
    memset(reader, 0, sizeof(SeaderReader));
    seader_reader_config_default(&reader->cfg);
    reader->lock = furi_mutex_alloc(FuriMutexTypeNormal);
    reader->resp_ready = furi_semaphore_alloc(1, 0);
    return reader;
}

static void seader_reader_free(SeaderReader* reader) {
    if(!reader) {
        return;
    }
    furi_semaphore_free(reader->resp_ready);
    furi_mutex_free(reader->lock);
    free(reader);
}

void seader_reader_run(Seader* seader) {
    furi_check(seader);
    SeaderWorker* worker = seader->worker;
    furi_check(worker);

    SeaderReader* reader = seader_reader_alloc();
    seader->reader = reader;

    /* Apply the persisted, user-editable identity (falls back to defaults). */
    strlcpy(reader->cfg.manufacturer, seader->reader_manufacturer, SEADER_READER_NAME_MAX);
    if(seader->reader_product[0]) {
        strlcpy(reader->cfg.product, seader->reader_product, SEADER_READER_NAME_MAX);
    }
    if(seader->reader_pid) {
        reader->cfg.pid = seader->reader_pid;
    }

    SeaderCcidReaderConfig ccfg = {
        .vid = reader->cfg.vid,
        .pid = reader->cfg.pid,
        .manuf = reader->cfg.manufacturer,
        .product = reader->cfg.product,
        .get_atr = seader_reader_get_atr,
        .xfr = seader_reader_xfr,
        .ctx = seader,
    };
    seader_usb_ccid_reader_start(&ccfg);

    uint32_t last_ccid = UINT32_MAX;
    while(seader_worker_get_state(worker) == SeaderWorkerStateReaderEmulation) {
        furi_delay_ms(50);
        /* Refresh the UI on any CCID activity (PowerOn/SlotStatus/XfrBlock/...),
           not just relayed APDUs, so the on-screen command indicator is live. */
        uint32_t ccid = seader_reader_ccid_count();
        if(ccid != last_ccid) {
            last_ccid = ccid;
            if(worker->callback) {
                worker->callback(SeaderWorkerEventReaderUpdate, worker->context);
            }
        }
    }

    seader_usb_ccid_reader_stop();
    seader->reader = NULL;
    seader_reader_free(reader);
}
