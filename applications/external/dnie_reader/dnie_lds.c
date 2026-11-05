#include "dnie_lds.h"

#include <string.h>

#define TAG   "DnieLds"
#define SW_OK 0x9000
#define LDS_READ_CHUNK \
    0x60 // 96 B en claro -> ~131 B protegidos, como las lecturas que ya funcionan
#define LDS_BUF 300

typedef struct {
    DnieTransport* poller;
    DnieSm* sm;
    BitBuffer* tx;
    BitBuffer* rx;
    DnieLdsStatus* status;
} LdsCtx;

static void lds_ctx_init(LdsCtx* c, DnieTransport* poller, DnieSm* sm, DnieLdsStatus* status) {
    c->poller = poller;
    c->sm = sm;
    c->tx = bit_buffer_alloc(LDS_BUF);
    c->rx = bit_buffer_alloc(LDS_BUF);
    c->status = status;
}

static void lds_ctx_free(LdsCtx* c) {
    bit_buffer_free(c->tx);
    bit_buffer_free(c->rx);
}

// Envia un comando protegido y devuelve los datos en claro. false = abortar (status relleno).
static bool lds_transceive(
    LdsCtx* c,
    const uint8_t hdr[4],
    const uint8_t* data,
    size_t len,
    int le,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    uint8_t apdu[LDS_BUF];
    size_t n = dnie_sm_wrap(c->sm, hdr, data, len, le, apdu, sizeof(apdu));
    if(n == 0) {
        c->status->result = DnieLdsErrorSm;
        c->status->sm_result = DnieSmErrorBuffer;
        return false;
    }

    uint16_t transport_sw;
    if(!dnie_card_apdu(c->poller, apdu, n, c->tx, c->rx, &transport_sw)) {
        c->status->result = DnieLdsErrorTransport;
        return false;
    }

    size_t rlen = bit_buffer_get_size_bytes(c->rx);
    DnieSmResult r = dnie_sm_unwrap(
        c->sm, bit_buffer_get_data(c->rx), rlen, out, out_max, out_len, &c->status->sw);
    c->status->sm_result = r;
    if(r != DnieSmOk) {
        FURI_LOG_E(TAG, "SM unwrap %d (SW transporte %04X)", r, transport_sw);
        if(r == DnieSmErrorPlainStatus) c->status->sw = transport_sw;
        c->status->result = DnieLdsErrorSm;
        return false;
    }
    return true;
}

bool dnie_lds_sm_command(
    DnieTransport* poller,
    DnieSm* sm,
    const uint8_t hdr[4],
    const uint8_t* data,
    size_t len,
    int le,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    DnieLdsStatus* status) {
    memset(status, 0, sizeof(*status));
    LdsCtx c;
    lds_ctx_init(&c, poller, sm, status);
    bool ok = lds_transceive(&c, hdr, data, len, le, out, out_max, out_len);
    lds_ctx_free(&c);
    return ok;
}

DnieLdsResult dnie_lds_select_app(DnieTransport* poller, DnieSm* sm, DnieLdsStatus* status) {
    memset(status, 0, sizeof(*status));
    LdsCtx c;
    lds_ctx_init(&c, poller, sm, status);
    uint8_t buf[16];
    size_t blen;

    // AID de la aplicacion ICAO eMRTD
    status->step = 1;
    const uint8_t sel_app[4] = {0x00, 0xA4, 0x04, 0x0C};
    const uint8_t aid[] = {0xA0, 0x00, 0x00, 0x02, 0x47, 0x10, 0x01};
    if(lds_transceive(&c, sel_app, aid, sizeof(aid), -1, buf, sizeof(buf), &blen) &&
       status->sw != SW_OK) {
        status->result = DnieLdsErrorSelectApp;
    }

    lds_ctx_free(&c);
    return status->result;
}

DnieLdsResult dnie_lds_read_file(
    DnieTransport* poller,
    DnieSm* sm,
    uint16_t fid,
    DnieLdsSink sink,
    DnieLdsProgress progress,
    void* context,
    size_t* total_out,
    DnieLdsStatus* status) {
    memset(status, 0, sizeof(*status));
    *total_out = 0;
    LdsCtx c;
    lds_ctx_init(&c, poller, sm, status);
    uint8_t chunk[LDS_READ_CHUNK + 16];
    size_t clen;

    do {
        // 1. SELECT EF por FID
        status->step = 2;
        const uint8_t sel_ef[4] = {0x00, 0xA4, 0x02, 0x0C};
        const uint8_t fid_b[] = {(uint8_t)(fid >> 8), (uint8_t)fid};
        if(!lds_transceive(&c, sel_ef, fid_b, sizeof(fid_b), -1, chunk, sizeof(chunk), &clen))
            break;
        if(status->sw != SW_OK) {
            status->result = DnieLdsErrorSelectFile;
            break;
        }

        // 2. Cabecera TLV (4 bytes) para saber cuanto mide
        status->step = 3;
        const uint8_t rb0[4] = {0x00, 0xB0, 0x00, 0x00};
        if(!lds_transceive(&c, rb0, NULL, 0, 4, chunk, sizeof(chunk), &clen)) break;
        if(status->sw != SW_OK || clen < 2) {
            status->result = DnieLdsErrorRead;
            break;
        }
        size_t total;
        if(chunk[1] < 0x80) {
            total = 2 + chunk[1];
        } else if(chunk[1] == 0x81 && clen >= 3) {
            total = 3 + chunk[2];
        } else if(chunk[1] == 0x82 && clen >= 4) {
            total = 4 + ((size_t)chunk[2] << 8 | chunk[3]);
        } else {
            status->result = DnieLdsErrorFormat;
            break;
        }
        if(total > DNIE_LDS_MAX_FILE) {
            status->result = DnieLdsErrorTooBig;
            break;
        }
        *total_out = total;

        size_t pos = clen < total ? clen : total;
        if(!sink(chunk, pos, context)) {
            status->result = DnieLdsErrorSink;
            break;
        }
        if(progress) progress(pos, total, context);

        // 3. Resto por trozos
        while(pos < total) {
            size_t want = total - pos;
            if(want > LDS_READ_CHUNK) want = LDS_READ_CHUNK;
            const uint8_t rb[4] = {0x00, 0xB0, (uint8_t)((pos >> 8) & 0x7F), (uint8_t)pos};
            if(!lds_transceive(&c, rb, NULL, 0, (int)want, chunk, sizeof(chunk), &clen)) break;
            if(status->sw != SW_OK || clen == 0) {
                status->result = DnieLdsErrorRead;
                break;
            }
            if(clen > total - pos) clen = total - pos;
            if(!sink(chunk, clen, context)) {
                status->result = DnieLdsErrorSink;
                break;
            }
            pos += clen;
            if(progress) progress(pos, total, context);
        }
    } while(false);

    memset(chunk, 0, sizeof(chunk));
    lds_ctx_free(&c);
    return status->result;
}

// --- DG1 en memoria ---

typedef struct {
    uint8_t* buf;
    size_t max;
    size_t len;
} MemSink;

static bool mem_sink(const uint8_t* data, size_t len, void* context) {
    MemSink* m = context;
    if(m->len + len > m->max) return false;
    memcpy(&m->buf[m->len], data, len);
    m->len += len;
    return true;
}

DnieLdsResult dnie_lds_read_dg1(
    DnieTransport* poller,
    DnieSm* sm,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    DnieLdsStatus* status) {
    MemSink m = {.buf = out, .max = out_max, .len = 0};
    size_t total;
    DnieLdsResult r =
        dnie_lds_read_file(poller, sm, DNIE_FID_DG1, mem_sink, NULL, &m, &total, status);
    if(r == DnieLdsOk && out_len) *out_len = m.len;
    if(r == DnieLdsOk && m.len >= 1 && out[0] != 0x61) {
        status->result = r = DnieLdsErrorFormat;
    }
    return r;
}

const char* dnie_lds_result_str(DnieLdsResult result) {
    switch(result) {
    case DnieLdsOk:
        return "OK";
    case DnieLdsErrorTransport:
        return "Radio error\n(don't move the ID)";
    case DnieLdsErrorSm:
        return "Secure channel\nfailure (SM)";
    case DnieLdsErrorSelectApp:
        return "Chip rejected\nthe eMRTD app";
    case DnieLdsErrorSelectFile:
        return "File\nnot found";
    case DnieLdsErrorRead:
        return "Read error";
    case DnieLdsErrorFormat:
        return "File has\nunexpected format";
    case DnieLdsErrorTooBig:
        return "File too\nlarge";
    case DnieLdsErrorSink:
        return "Could not save\n(SD card?)";
    }
    return "Unknown error";
}
