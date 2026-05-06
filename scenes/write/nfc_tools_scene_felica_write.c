#include "../../include/nfc_tools_i.h"
#include <nfc/nfc_poller.h>
#include <nfc/protocols/felica/felica.h>
#include <toolbox/bit_buffer.h>
#include <notification/notification_messages.h>

// ── Constantes FeliCa ─────────────────────────────────────────────────────────

#define FELICA_CMD_WRITE_WITHOUT_ENCRYPTION (0x08U)
#define FELICA_RESP_WRITE                   (0x09U) // 0x08 | 0x01
#define FELICA_SERVICE_RW                   (0x0009U)
#define FELICA_FWT                          (200000U)
#define FELICA_IDM_SIZE                     (8U)
#define FELICA_BLOCK_SIZE                   (16U)

// Frame size before CRC :
//  1 (len) + 1 (cmd) + 8 (IDm) + 1 (NumSvc) + 2 (SvcCode) + 1 (NumBlk) + 2 (BlkList) + 16 (data)
#define FELICA_WRITE_FRAME_SIZE             (32U)

// ── Helper: hex nibble decode ────────────────────────────────────────────────

static int felica_hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Decodes s[2..33] (32 hex chars) into 16 bytes in out
static bool felica_parse_data_bytes(const char* s, uint8_t* out) {
    for(uint8_t i = 0; i < 16; i++) {
        int hi = felica_hex_nibble(s[2 + i * 2]);
        int lo = felica_hex_nibble(s[2 + i * 2 + 1]);
        if(hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// ── Contexte du worker ────────────────────────────────────────────────────────

typedef struct {
    NfcToolsApp*   app;
    NfcPoller*     nfc_poller;
    FuriEventFlag* done;
    uint8_t        block;
    uint8_t        data[FELICA_BLOCK_SIZE];
    bool           success;
} FelicaWriteCtx;

// ── Callback NfcPoller ────────────────────────────────────────────────────────

static NfcCommand nfc_tools_felica_write_cb(NfcGenericEvent event, void* context) {
    FelicaWriteCtx*    ctx = context;
    FelicaPollerEvent* ev  = event.event_data;

    if(ev->type == FelicaPollerEventTypeRequestAuthContext) {
        ev->data->auth_context->skip_auth = true;
        return NfcCommandContinue;
    }

    if(ev->type == FelicaPollerEventTypeReady ||
       ev->type == FelicaPollerEventTypeIncomplete) {

        NfcToolsApp* app = ctx->app;

        // Retrieve the IDm from data read by the poller
        const FelicaData* felica =
            (const FelicaData*)nfc_poller_get_data(ctx->nfc_poller);
        size_t idm_len = 0;
        const uint8_t* idm = felica_get_uid(felica, &idm_len);
        if(!idm || idm_len < FELICA_IDM_SIZE) {
            furi_string_set(app->info_str, "Tag error\nCould not get IDm");
            furi_event_flag_set(ctx->done, 1u);
            return NfcCommandStop;
        }

        // Construire la trame WRITE WITHOUT ENCRYPTION (32 bytes avant CRC)
        // [LEN][CMD][IDm×8][NumSvc=1][SvcCode×2][NumBlk=1][BlkElem×2][Data×16]
        BitBuffer* tx = bit_buffer_alloc(FELICA_WRITE_FRAME_SIZE + 2); // +2 pour CRC
        BitBuffer* rx = bit_buffer_alloc(64);

        uint8_t svc_lo = (uint8_t)(FELICA_SERVICE_RW & 0xFF);
        uint8_t svc_hi = (uint8_t)((FELICA_SERVICE_RW >> 8) & 0xFF);

        bit_buffer_append_byte(tx, FELICA_WRITE_FRAME_SIZE); // length = 32
        bit_buffer_append_byte(tx, FELICA_CMD_WRITE_WITHOUT_ENCRYPTION);
        bit_buffer_append_bytes(tx, idm, FELICA_IDM_SIZE);
        bit_buffer_append_byte(tx, 1);       // NumServices = 1
        bit_buffer_append_byte(tx, svc_lo);  // ServiceCode LE low
        bit_buffer_append_byte(tx, svc_hi);  // ServiceCode LE high
        bit_buffer_append_byte(tx, 1);       // NumBlocks = 1
        bit_buffer_append_byte(tx, 0x80);    // BlockListElement[0]: 2-byte, svc_idx=0
        bit_buffer_append_byte(tx, ctx->block); // block number
        bit_buffer_append_bytes(tx, ctx->data, FELICA_BLOCK_SIZE);

        // Ajouter le CRC FeliCa (2 bytes)
        felica_crc_append(tx);

        // Transmettre la trame
        NfcError nfc_err = nfc_poller_trx(app->nfc, tx, rx, FELICA_FWT);

        if(nfc_err == NfcErrorNone && felica_crc_check(rx)) {
            felica_crc_trim(rx);

            size_t rx_len = bit_buffer_get_size_bytes(rx);
            const uint8_t* rx_data = bit_buffer_get_data(rx);

            // Minimum response: 12 bytes (len + resp_code + IDm + SF1 + SF2)
            if(rx_len >= 12 && rx_data[1] == FELICA_RESP_WRITE) {
                uint8_t sf1 = rx_data[10];
                uint8_t sf2 = rx_data[11];
                if(sf1 == 0 && sf2 == 0) {
                    ctx->success = true;
                    furi_string_printf(
                        app->info_str,
                        "Block S%02u written\nOK",
                        (unsigned)ctx->block);
                } else {
                    furi_string_printf(
                        app->info_str,
                        "Write denied\nBlock S%02u\nSF1=%02X SF2=%02X",
                        (unsigned)ctx->block,
                        (unsigned)sf1,
                        (unsigned)sf2);
                }
            } else {
                furi_string_printf(
                    app->info_str,
                    "Bad response\nlen=%u code=%02X",
                    (unsigned)rx_len,
                    rx_len > 1 ? (unsigned)rx_data[1] : 0U);
            }
        } else {
            furi_string_printf(
                app->info_str,
                "NFC error\ncode %d",
                (int)nfc_err);
        }

        bit_buffer_free(tx);
        bit_buffer_free(rx);

    } else {
        // FelicaPollerEventTypeError
        furi_string_set(ctx->app->info_str, "Tag error\nCould not\nactivate FeliCa");
    }

    furi_event_flag_set(ctx->done, 1u);
    return NfcCommandStop;
}

// ── Worker ────────────────────────────────────────────────────────────────────

static int32_t nfc_tools_felica_write_worker(void* context) {
    NfcToolsApp* app = context;

    // Decode data bytes from ndef_buf1[2..33]
    uint8_t data_bytes[FELICA_BLOCK_SIZE];
    if(!felica_parse_data_bytes(app->ndef_buf1, data_bytes)) {
        furi_string_set(app->info_str, NTS_ERR_INVALID_HEX);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
        return 0;
    }

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    FelicaWriteCtx wctx = {
        .app     = app,
        .done    = furi_event_flag_alloc(),
        .block   = app->felica_write_block,
        .success = false,
    };
    memcpy(wctx.data, data_bytes, FELICA_BLOCK_SIZE);

    wctx.nfc_poller = nfc_poller_alloc(app->nfc, NfcProtocolFelica);
    nfc_poller_start(wctx.nfc_poller, nfc_tools_felica_write_cb, &wctx);

    furi_event_flag_wait(wctx.done, 1u, FuriFlagWaitAny, 8000);

    nfc_poller_stop(wctx.nfc_poller);
    nfc_poller_free(wctx.nfc_poller);
    furi_event_flag_free(wctx.done);

    if(furi_event_flag_get(app->worker_flags) & NFC_TOOLS_WORKER_FLAG_STOP) return 0;

    if(wctx.success) {
        notification_message(app->notifications, &sequence_success);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteSuccess);
    } else {
        notification_message(app->notifications, &sequence_error);
        view_dispatcher_send_custom_event(app->view_dispatcher, NfcToolsEventWriteFail);
    }

    return 0;
}

// ── Stop helper ───────────────────────────────────────────────────────────────

static void nfc_tools_felica_write_stop_worker(NfcToolsApp* app) {
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

void nfc_tools_scene_felica_write_on_enter(void* context) {
    NfcToolsApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, NTS_WRITE_TITLE_FELICA,
                     64, 10, AlignCenter, AlignCenter);
    popup_set_text(app->popup, NTS_POPUP_APPROACH_TAG,
                   64, 35, AlignCenter, AlignCenter);

    furi_string_reset(app->info_str);

    app->worker_flags = furi_event_flag_alloc();
    app->worker_thread =
        furi_thread_alloc_ex("NfcFelicaWrite", 2 * 1024, nfc_tools_felica_write_worker, app);
    furi_thread_start(app->worker_thread);

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcToolsViewPopup);
}

bool nfc_tools_scene_felica_write_on_event(void* context, SceneManagerEvent event) {
    NfcToolsApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == NfcToolsEventWriteSuccess) {
            popup_set_header(app->popup, NTS_STATUS_WRITE_COMPLETE,
                             64, 10, AlignCenter, AlignCenter);
            popup_set_text(app->popup, furi_string_get_cstr(app->info_str),
                           64, 35, AlignCenter, AlignCenter);
            consumed = true;
        } else if(event.event == NfcToolsEventWriteFail) {
            popup_set_header(app->popup, NTS_ERR_FAILED,
                             64, 10, AlignCenter, AlignCenter);
            popup_set_text(app->popup, furi_string_get_cstr(app->info_str),
                           64, 35, AlignCenter, AlignCenter);
            consumed = true;
        }
    }

    return consumed;
}

void nfc_tools_scene_felica_write_on_exit(void* context) {
    NfcToolsApp* app = context;
    nfc_tools_felica_write_stop_worker(app);
    popup_reset(app->popup);
}
