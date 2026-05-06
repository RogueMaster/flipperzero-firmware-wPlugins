#include "../include/nfc_tools_icode.h"
#include <nfc/nfc_poller.h>

// ── FWT for ISO 15693 commands ───────────────────────────────────────────────
#define ISO15693_FORMAT_FWT_FC      (500000U)
#define ISO15693_NDEF_WRITE_FWT_FC  (500000U)

// ─────────────────────────────────────────────────────────────────────────────
// ISO 15693 frame helpers (non-addressed)
// ─────────────────────────────────────────────────────────────────────────────

static void iso15693_build_write_frame_fmt(
    BitBuffer* tx, uint8_t block_num, const uint8_t* data) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, ISO15693_3_REQ_FLAG_DATA_RATE_HI);
    bit_buffer_append_byte(tx, ISO15693_3_CMD_WRITE_BLOCK);
    bit_buffer_append_byte(tx, block_num);
    bit_buffer_append_bytes(tx, data, 4);
}

static void iso15693_build_read_frame_fmt(BitBuffer* tx, uint8_t block_num) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, ISO15693_3_REQ_FLAG_DATA_RATE_HI);
    bit_buffer_append_byte(tx, ISO15693_3_CMD_READ_BLOCK);
    bit_buffer_append_byte(tx, block_num);
}

// ─────────────────────────────────────────────────────────────────────────────
// FORMAT
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    bool           success;
} Iso15693FormatCtx;

static NfcCommand nfc_tools_iso15693_format_cb(NfcGenericEvent event, void* context) {
    Iso15693FormatCtx* ctx = context;
    const Iso15693_3PollerEvent* ev = event.event_data;

    if(ev->type == Iso15693_3PollerEventTypeReady) {
        NfcToolsApp* app = ctx->app;
        Iso15693_3Poller* iso_poller = (Iso15693_3Poller*)event.instance;

        BitBuffer* tx = bit_buffer_alloc(7);
        BitBuffer* rx = bit_buffer_alloc(16);

        static const uint8_t zeros[4] = {0x00, 0x00, 0x00, 0x00};

        bool     ok      = true;
        uint16_t written = 0;

        for(uint16_t b = 0; b <= 0xFF; b++) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
                ok = false;
                break;
            }

            iso15693_build_write_frame_fmt(tx, (uint8_t)b, zeros);
            Iso15693_3Error werr = iso15693_3_poller_send_frame(
                iso_poller, tx, rx, ISO15693_FORMAT_FWT_FC);

            if(werr == Iso15693_3ErrorNone) {
                size_t rx_len = bit_buffer_get_size_bytes(rx);
                if(rx_len >= 1 && (bit_buffer_get_data(rx)[0] & 0x01)) {
                    uint8_t ec = (rx_len >= 2) ? bit_buffer_get_data(rx)[1] : 0xFF;
                    if(ec == ISO15693_3_RESP_ERROR_BLOCK_UNAVAILABLE) break;
                    if(ec == ISO15693_3_RESP_ERROR_BLOCK_ALREADY_LOCKED ||
                       ec == ISO15693_3_RESP_ERROR_BLOCK_LOCKED) {
                        written++;
                        continue;
                    }
                    furi_string_printf(
                        app->info_str, "Write error blk %u\ncode 0x%02X",
                        (unsigned)b, (unsigned)ec);
                    ok = false;
                    break;
                }
                written++;
                continue;
            } else if(werr != Iso15693_3ErrorTimeout) {
                furi_string_printf(
                    app->info_str, "NFC error blk %u\n(code %d)",
                    (unsigned)b, (int)werr);
                ok = false;
                break;
            }

            iso15693_build_read_frame_fmt(tx, (uint8_t)b);
            Iso15693_3Error rerr = iso15693_3_poller_send_frame(
                iso_poller, tx, rx, ISO15693_FORMAT_FWT_FC);

            if(rerr == Iso15693_3ErrorNone) {
                size_t rx_len = bit_buffer_get_size_bytes(rx);
                if(rx_len >= 1 && (bit_buffer_get_data(rx)[0] & 0x01)) {
                    uint8_t ec = (rx_len >= 2) ? bit_buffer_get_data(rx)[1] : 0xFF;
                    if(ec == ISO15693_3_RESP_ERROR_BLOCK_UNAVAILABLE) {
                        break;
                    }
                }
                written++;
            } else if(rerr == Iso15693_3ErrorTimeout) {
                break;
            } else {
                furi_string_printf(
                    app->info_str, "Read error blk %u\n(code %d)",
                    (unsigned)b, (int)rerr);
                ok = false;
                break;
            }
        }

        bit_buffer_free(tx);
        bit_buffer_free(rx);

        if(ok) {
            furi_string_printf(
                app->info_str,
                "Memory formatted!\n0x00-0x%02X\nBack to exit",
                written > 0 ? (unsigned)(written - 1) : 0u);
        }
        ctx->success = ok;

    } else if(ev->type == Iso15693_3PollerEventTypeError) {
        furi_string_set(ctx->app->info_str, "Tag read error");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

static NfcCommand nfc_tools_slix_format_cb(NfcGenericEvent event, void* context) {
    Iso15693FormatCtx* ctx = context;
    const SlixPollerEvent* ev = (const SlixPollerEvent*)event.event_data;

    if(ev->type == SlixPollerEventTypeReady) {
        NfcToolsApp* app         = ctx->app;
        SlixPoller* slix_poller  = (SlixPoller*)event.instance;
        const SlixData* slix     = (const SlixData*)nfc_poller_get_data(ctx->poller);
        const Iso15693_3Data* iso = slix_get_base_data(slix);
        uint16_t block_count     = iso15693_3_get_block_count(iso);
        uint8_t  block_size      = iso15693_3_get_block_size(iso);

        if(block_count == 0 || block_size == 0) {
            furi_string_set(app->info_str, "Invalid memory");
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }

        BitBuffer* tx  = bit_buffer_alloc(7);
        BitBuffer* rx  = bit_buffer_alloc(16);

        bool need_write_pwd = false;
        for(uint16_t b = 0; b < block_count && !need_write_pwd; b++) {
            if(slix_is_block_protected(slix, SlixPasswordTypeWrite, (uint8_t)b))
                need_write_pwd = true;
        }
        if(need_write_pwd) {
            SlixRandomNumber rn = 0;
            if(slix_poller_get_random_number(slix_poller, &rn) == SlixErrorNone) {
                slix_poller_set_password(
                    slix_poller, SlixPasswordTypeWrite, (SlixPassword)0x00000000, rn);
            }
        }

        static const uint8_t zeros[4] = {0x00, 0x00, 0x00, 0x00};
        bool     ok      = true;
        uint16_t written = 0;

        for(uint16_t b = 0; b <= 0xFF; b++) {
            if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
                ok = false; break;
            }

            iso15693_build_write_frame_fmt(tx, (uint8_t)b, zeros);
            SlixError werr = slix_poller_send_frame(slix_poller, tx, rx, ISO15693_FORMAT_FWT_FC);

            if(werr == SlixErrorNone) {
                size_t rx_len = bit_buffer_get_size_bytes(rx);
                if(rx_len >= 1 && (bit_buffer_get_data(rx)[0] & 0x01)) {
                    uint8_t ec = (rx_len >= 2) ? bit_buffer_get_data(rx)[1] : 0xFF;
                    if(ec == ISO15693_3_RESP_ERROR_BLOCK_UNAVAILABLE) break;
                    if(ec == ISO15693_3_RESP_ERROR_BLOCK_ALREADY_LOCKED ||
                       ec == ISO15693_3_RESP_ERROR_BLOCK_LOCKED) {
                        written++;
                        continue;
                    }
                    furi_string_printf(
                        app->info_str, "Write error blk %u\ncode 0x%02X",
                        (unsigned)b, (unsigned)ec);
                    ok = false; break;
                }
                written++;
                continue;
            } else if(werr != SlixErrorTimeout) {
                furi_string_printf(
                    app->info_str, "NFC error blk %u\n(slix %d)",
                    (unsigned)b, (int)werr);
                ok = false; break;
            }

            iso15693_build_read_frame_fmt(tx, (uint8_t)b);
            SlixError rerr = slix_poller_send_frame(slix_poller, tx, rx, ISO15693_FORMAT_FWT_FC);

            if(rerr == SlixErrorNone) {
                size_t rx_len = bit_buffer_get_size_bytes(rx);
                if(rx_len >= 1 && (bit_buffer_get_data(rx)[0] & 0x01)) {
                    uint8_t ec = (rx_len >= 2) ? bit_buffer_get_data(rx)[1] : 0xFF;
                    if(ec == ISO15693_3_RESP_ERROR_BLOCK_UNAVAILABLE) {
                        break;
                    }
                }
                written++;
            } else if(rerr == SlixErrorTimeout) {
                break;
            } else {
                furi_string_printf(
                    app->info_str, "Read error blk %u\n(slix %d)",
                    (unsigned)b, (int)rerr);
                ok = false; break;
            }
        }

        bit_buffer_free(tx);
        bit_buffer_free(rx);

        if(ok) {
            furi_string_printf(
                app->info_str,
                "Memory formatted!\n0x00-0x%02X\nBack to exit",
                written > 0 ? (unsigned)(written - 1) : 0u);
        }
        ctx->success = ok;

    } else if(ev->type == SlixPollerEventTypePrivacyUnlockRequest) {
        ((SlixPollerEvent*)event.event_data)->data->privacy_password.password_set = false;
        return NfcCommandContinue;
    } else {
        furi_string_set(ctx->app->info_str, "Tag error");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

bool nfc_tools_icode_format(NfcToolsApp* app, bool use_slix) {
    Iso15693FormatCtx iso_ctx = {
        .app     = app,
        .done    = furi_event_flag_alloc(),
        .success = false,
    };
    NfcGenericCallback cb = use_slix
        ? (NfcGenericCallback)nfc_tools_slix_format_cb
        : (NfcGenericCallback)nfc_tools_iso15693_format_cb;
    iso_ctx.poller = nfc_poller_alloc(
        app->nfc, use_slix ? NfcProtocolSlix : NfcProtocolIso15693_3);
    nfc_poller_start(iso_ctx.poller, cb, &iso_ctx);

    furi_event_flag_wait(iso_ctx.done, 1u, FuriFlagWaitAny, 15000);

    nfc_poller_stop(iso_ctx.poller);
    nfc_poller_free(iso_ctx.poller);
    furi_event_flag_free(iso_ctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;
    return iso_ctx.success;
}

// ─────────────────────────────────────────────────────────────────────────────
// DUMP
// ─────────────────────────────────────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    bool           success;
} Iso15693SectorCtx;

static NfcCommand nfc_tools_iso15693_sector_cb(NfcGenericEvent event, void* context) {
    Iso15693SectorCtx* ctx = context;
    const Iso15693_3PollerEvent* ev = event.event_data;

    if(ev->type == Iso15693_3PollerEventTypeReady) {
        NfcToolsApp* app = ctx->app;
        const Iso15693_3Data* iso =
            (const Iso15693_3Data*)nfc_poller_get_data(ctx->poller);
        uint16_t bc = iso15693_3_get_block_count(iso);
        uint8_t  bs = iso15693_3_get_block_size(iso);

        furi_string_cat_printf(
            app->info_str, "ISO 15693\n%u blocks x %u bytes\n\n", (unsigned)bc, (unsigned)bs);
        furi_string_cat_printf(
            app->ndef_str, "ISO 15693 - ASCII\n\n");

        if(bc > 0 && bs > 0) {
            for(uint16_t b = 0; b < bc; b++) {
                const uint8_t* d = iso15693_3_get_block_data(iso, (uint8_t)b);

                furi_string_cat_printf(app->info_str, "[%02X]", (unsigned)b);
                for(uint8_t i = 0; i < bs; i++) {
                    furi_string_cat_printf(app->info_str, " %02X", d[i]);
                }
                furi_string_cat_str(app->info_str, "\n");

                furi_string_cat_printf(app->ndef_str, "[%02X] ", (unsigned)b);
                for(uint8_t i = 0; i < bs; i++) {
                    char c = (char)d[i];
                    furi_string_cat_printf(app->ndef_str, "%c",
                        (c >= 32 && c < 127) ? c : '.');
                }
                furi_string_cat_str(app->ndef_str, "\n");
            }
        } else {
            furi_string_cat_str(app->info_str, "(no accessible block)");
        }
        ctx->success = true;
    } else if(ev->type == Iso15693_3PollerEventTypeError) {
        furi_string_set(ctx->app->info_str, "Read error\nISO 15693");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

bool nfc_tools_icode_dump(NfcToolsApp* app) {
    Iso15693SectorCtx iso_ctx = {
        .app     = app,
        .done    = furi_event_flag_alloc(),
        .success = false,
    };
    iso_ctx.poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    nfc_poller_start(iso_ctx.poller, nfc_tools_iso15693_sector_cb, &iso_ctx);

    furi_event_flag_wait(iso_ctx.done, 1u, FuriFlagWaitAny, 5000);

    nfc_poller_stop(iso_ctx.poller);
    nfc_poller_free(iso_ctx.poller);
    furi_event_flag_free(iso_ctx.done);

    return iso_ctx.success;
}

// ─────────────────────────────────────────────────────────────────────────────
// NDEF WRITE
// ─────────────────────────────────────────────────────────────────────────────

typedef enum {
    NfcVExOk,
    NfcVExTimeout,
    NfcVExError,
} NfcVExResult;

typedef NfcVExResult (*NfcVExFn)(BitBuffer* tx, BitBuffer* rx, uint32_t fwt, void* ctx);

typedef struct { Iso15693_3Poller* poller; } NfcVIso15693Ctx;

static NfcVExResult nfcv_ex_iso15693(BitBuffer* tx, BitBuffer* rx, uint32_t fwt, void* ctx) {
    NfcVIso15693Ctx* c = ctx;
    Iso15693_3Error err = iso15693_3_poller_send_frame(c->poller, tx, rx, fwt);
    if(err == Iso15693_3ErrorNone)    return NfcVExOk;
    if(err == Iso15693_3ErrorTimeout) return NfcVExTimeout;
    return NfcVExError;
}

typedef struct { SlixPoller* poller; } NfcVSlixCtx;

static NfcVExResult nfcv_ex_slix(BitBuffer* tx, BitBuffer* rx, uint32_t fwt, void* ctx) {
    NfcVSlixCtx* c = ctx;
    SlixError err = slix_poller_send_frame(c->poller, tx, rx, fwt);
    if(err == SlixErrorNone)    return NfcVExOk;
    if(err == SlixErrorTimeout) return NfcVExTimeout;
    return NfcVExError;
}

static void nfcv_build_read_blk(BitBuffer* tx, uint8_t blk) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, ISO15693_3_REQ_FLAG_DATA_RATE_HI);
    bit_buffer_append_byte(tx, ISO15693_3_CMD_READ_BLOCK);
    bit_buffer_append_byte(tx, blk);
}

static void nfcv_build_write_blk(
    BitBuffer* tx, uint8_t blk, const uint8_t* data, uint8_t block_size) {
    bit_buffer_reset(tx);
    bit_buffer_append_byte(tx, ISO15693_3_REQ_FLAG_DATA_RATE_HI);
    bit_buffer_append_byte(tx, ISO15693_3_CMD_WRITE_BLOCK);
    bit_buffer_append_byte(tx, blk);
    bit_buffer_append_bytes(tx, data, block_size);
}

static bool iso15693_write_ndef_blocks(
    NfcToolsApp*   app,
    BitBuffer*     tx,
    BitBuffer*     rx,
    uint8_t        block_size,
    uint16_t       block_count,
    const uint8_t* ndef_buf,
    size_t         ndef_size,
    NfcVExFn       ex_fn,
    void*          ex_ctx)
{
    size_t user_capacity = (size_t)(block_count - 1) * block_size;
    if(ndef_size > user_capacity) {
        furi_string_printf(
            app->info_str,
            "Tag too small!\n%u bytes needed\n%u available",
            (unsigned)ndef_size, (unsigned)user_capacity);
        return false;
    }

    uint8_t mlen = (uint8_t)(((uint32_t)(block_count - 1) * block_size) >> 3);
    const uint8_t cc_expected[4] = {0xE1, 0x40, mlen, 0x00};

    uint8_t  cc_current[4] = {0};
    bool     cc_read_ok    = false;
    bool     cc_has_magic  = false;

    nfcv_build_read_blk(tx, 0);
    NfcVExResult r = ex_fn(tx, rx, ISO15693_NDEF_WRITE_FWT_FC, ex_ctx);

    if(r == NfcVExOk) {
        size_t rx_len = bit_buffer_get_size_bytes(rx);
        const uint8_t* rx_data = bit_buffer_get_data(rx);
        if(rx_len >= (size_t)(1 + 4) && !(rx_data[0] & 0x01)) {
            memcpy(cc_current, rx_data + 1, 4);
            cc_read_ok   = true;
            cc_has_magic = (cc_current[0] == 0xE1);
        }
    }

    bool cc_valid = cc_read_ok && cc_has_magic && (cc_current[2] == mlen);

    if(!cc_valid) {
        nfcv_build_write_blk(tx, 0, cc_expected, block_size);
        r = ex_fn(tx, rx, ISO15693_NDEF_WRITE_FWT_FC, ex_ctx);

        if(r == NfcVExError) {
            size_t rx_len = bit_buffer_get_size_bytes(rx);
            const uint8_t* rx_data = bit_buffer_get_data(rx);
            bool app_err = (rx_len >= 2) && (rx_data[0] & 0x01);
            uint8_t ec = app_err ? rx_data[1] : 0xFF;

            if(!cc_has_magic) {
                furi_string_printf(
                    app->info_str,
                    "CC write failed\nBlk 0 locked?\ncode 0x%02X",
                    (unsigned)ec);
                return false;
            }
        }
    }

    size_t   num_blocks = (ndef_size + block_size - 1) / block_size;
    uint8_t* padded     = malloc(num_blocks * block_size);
    if(!padded) {
        furi_string_set(app->info_str, "Out of memory");
        return false;
    }
    memset(padded, 0x00, num_blocks * block_size);
    memcpy(padded, ndef_buf, ndef_size);

    bool ok = true;
    for(size_t i = 0; i < num_blocks && ok; i++) {
        if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) {
            ok = false;
            break;
        }
        nfcv_build_write_blk(tx, (uint8_t)(1 + i), padded + i * block_size, block_size);
        r = ex_fn(tx, rx, ISO15693_NDEF_WRITE_FWT_FC, ex_ctx);

        if(r == NfcVExError) {
            size_t rx_len = bit_buffer_get_size_bytes(rx);
            const uint8_t* rx_data = bit_buffer_get_data(rx);
            if(rx_len >= 2 && (rx_data[0] & 0x01)) {
                furi_string_printf(
                    app->info_str, "Write failed blk %u\ncode 0x%02X",
                    (unsigned)(1 + i), (unsigned)rx_data[1]);
            } else {
                furi_string_printf(
                    app->info_str, "Write failed\nblock %u", (unsigned)(1 + i));
            }
            ok = false;
        }
    }

    free(padded);

    if(ok) {
        furi_string_printf(
            app->info_str, "%u bytes written\nBack to exit", (unsigned)ndef_size);
    }
    return ok;
}

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     poller;
    FuriEventFlag* done;
    const uint8_t* ndef_buf;
    size_t         ndef_size;
    bool           success;
} Iso15693NdefWriteCtx;

static NfcCommand nfc_tools_iso15693_ndef_write_cb(NfcGenericEvent event, void* context) {
    Iso15693NdefWriteCtx*        ctx = context;
    const Iso15693_3PollerEvent* ev  = event.event_data;

    if(ev->type == Iso15693_3PollerEventTypeReady) {
        NfcToolsApp*          app         = ctx->app;
        const Iso15693_3Data* iso         = (const Iso15693_3Data*)nfc_poller_get_data(ctx->poller);
        uint16_t              block_count = iso15693_3_get_block_count(iso);
        uint8_t               block_size  = iso15693_3_get_block_size(iso);

        if(block_count < 2 || block_size < 4) {
            furi_string_set(app->info_str, "Invalid tag\n(need >=2 blocks)");
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }

        BitBuffer*      tx     = bit_buffer_alloc(block_size + 3);
        BitBuffer*      rx     = bit_buffer_alloc(block_size + 4);
        NfcVIso15693Ctx ex_ctx = { .poller = (Iso15693_3Poller*)event.instance };

        ctx->success = iso15693_write_ndef_blocks(
            app, tx, rx, block_size, block_count,
            ctx->ndef_buf, ctx->ndef_size,
            nfcv_ex_iso15693, &ex_ctx);

        bit_buffer_free(tx);
        bit_buffer_free(rx);
    } else {
        furi_string_set(ctx->app->info_str, "Tag error");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

static NfcCommand nfc_tools_slix_ndef_write_cb(NfcGenericEvent event, void* context) {
    Iso15693NdefWriteCtx*  ctx = context;
    const SlixPollerEvent* ev  = (const SlixPollerEvent*)event.event_data;

    if(ev->type == SlixPollerEventTypeReady) {
        NfcToolsApp*          app         = ctx->app;
        SlixPoller*           slix_poller = (SlixPoller*)event.instance;
        const SlixData*       slix        = (const SlixData*)nfc_poller_get_data(ctx->poller);
        const Iso15693_3Data* iso         = slix_get_base_data(slix);
        uint16_t              block_count = iso15693_3_get_block_count(iso);
        uint8_t               block_size  = iso15693_3_get_block_size(iso);

        if(block_count < 2 || block_size < 4) {
            furi_string_set(app->info_str, "Invalid tag\n(need >=2 blocks)");
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }

        bool need_write_pwd = false;
        for(uint16_t b = 0; b < block_count && !need_write_pwd; b++) {
            if(slix_is_block_protected(slix, SlixPasswordTypeWrite, (uint8_t)b))
                need_write_pwd = true;
        }
        if(need_write_pwd) {
            SlixRandomNumber rn = 0;
            if(slix_poller_get_random_number(slix_poller, &rn) == SlixErrorNone) {
                slix_poller_set_password(
                    slix_poller, SlixPasswordTypeWrite, (SlixPassword)0x00000000, rn);
            }
        }

        BitBuffer*   tx     = bit_buffer_alloc(block_size + 3);
        BitBuffer*   rx     = bit_buffer_alloc(block_size + 4);
        NfcVSlixCtx  ex_ctx = { .poller = slix_poller };

        ctx->success = iso15693_write_ndef_blocks(
            app, tx, rx, block_size, block_count,
            ctx->ndef_buf, ctx->ndef_size,
            nfcv_ex_slix, &ex_ctx);

        bit_buffer_free(tx);
        bit_buffer_free(rx);

    } else if(ev->type == SlixPollerEventTypePrivacyUnlockRequest) {
        ((SlixPollerEvent*)event.event_data)->data->privacy_password.password_set = false;
        return NfcCommandContinue;
    } else {
        furi_string_set(ctx->app->info_str, "Tag error");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

bool nfc_tools_icode_write_ndef(
    NfcToolsApp* app,
    const uint8_t* ndef_data,
    size_t ndef_size,
    bool use_slix)
{
    Iso15693NdefWriteCtx iso_ctx = {
        .app       = app,
        .done      = furi_event_flag_alloc(),
        .ndef_buf  = ndef_data,
        .ndef_size = ndef_size,
        .success   = false,
    };
    NfcGenericCallback cb = use_slix
        ? (NfcGenericCallback)nfc_tools_slix_ndef_write_cb
        : (NfcGenericCallback)nfc_tools_iso15693_ndef_write_cb;
    iso_ctx.poller = nfc_poller_alloc(
        app->nfc, use_slix ? NfcProtocolSlix : NfcProtocolIso15693_3);
    nfc_poller_start(iso_ctx.poller, cb, &iso_ctx);

    furi_event_flag_wait(iso_ctx.done, 1u, FuriFlagWaitAny, 10000);

    nfc_poller_stop(iso_ctx.poller);
    nfc_poller_free(iso_ctx.poller);
    furi_event_flag_free(iso_ctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return false;
    return iso_ctx.success;
}
