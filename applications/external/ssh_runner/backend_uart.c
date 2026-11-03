/*
 * UART backend: the real link to the ESP32 over USART1 (pins 13/14), using the
 * shared proto/ codec and reliability layer.
 *
 * Owns USART1 for the app's lifetime: the Expansion service is disabled before the
 * port is acquired and re-enabled after it is released, on every exit path.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "ssh_runner_i.h"

#include <expansion/expansion.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <proto/proto_link.h>

#define UART_QUEUE     4
#define HELLO_RETRY_MS 2000
#define KEEPALIVE_MS   700 /* the ESP32 drops a peer silent for 2 s */
#define RX_BUF         1024 /* a 1 MiB echo at 921600 baud runs clean with 1024 */
#define BAUD_SWITCH_MS 20

/* A request waiting for the link (stop-and-wait). The payload is allocated at its own size
 * (most are a few bytes; a full 512 only for script and key chunks) and wiped and freed once
 * the link holds the encoded frame: 4 x 512 bytes kept for good were 2 KB of the heap. */
typedef struct {
    uint8_t type;
    uint16_t len;
    uint8_t* payload;
} QueuedReq;

typedef enum {
    ScriptIdle,
    ScriptBegin,
    ScriptChunks,
    ScriptEnd,
} ScriptPhase;

typedef struct {
    BackendEventCallback cb;
    void* ctx;
    Expansion* expansion;
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;
    ProtoLink link;
    uint32_t now;

    QueuedReq q[UART_QUEUE];
    uint8_t q_head, q_count;

    uint8_t link_state; /* BackendLinkState */
    bool hello_inflight;
    uint32_t hello_at;
    uint32_t baud;
    uint32_t tx_bytes;
    uint32_t rx_bytes;
    volatile uint32_t rx_dropped; /* by the RX interrupt, buffer full */
    uint32_t last_tx; /* keepalive: an empty ECHO after KEEPALIVE_MS of silence */
    uint32_t switch_baud; /* pending switch after HELLO_RESP */
    uint32_t switch_at;
    char fw_version[PROTO_STR_VERSION + 1];

    ScriptPhase script;
    bool script_is_key; /* KEY_BLOB_* instead of SCRIPT_*, then the pending SSH_CONNECT */
    uint32_t script_total, script_off, script_crc;
    BackendScriptRead script_read;
    void* script_ctx;
    ProtoSshConnect connect_req; /* SSH_CONNECT after a staged key */
    bool connect_pending;

    /* E->F key file (KEY_GEN) and public key line, reassembled */
    uint8_t* in_blob;
    uint32_t in_total, in_off, in_crc;
    char* in_pub;
    uint16_t in_pub_off;

    /* link test (Developer menu): one ECHO outstanding, its payload kept for the compare */
    bool lt_active;
    BackendLinkTest lt;
    uint32_t lt_start, lt_retx0;
    uint8_t* lt_buf; /* LT_LEN bytes while a link test runs, else NULL */
} UartBackend;

#define LT_LEN PROTO_MAX_PAYLOAD

static void q_drop(QueuedReq* r) {
    if(r->payload) {
        memset(r->payload, 0, r->len); /* may hold a key chunk or a passphrase */
        free(r->payload);
        r->payload = NULL;
    }
    r->len = 0;
}

static void lt_end(UartBackend* u) {
    u->lt_active = false;
    free(u->lt_buf);
    u->lt_buf = NULL;
}

static void emit(UartBackend* u, const BackendEvent* e) {
    if(u->cb) u->cb(u->ctx, e);
}

static void emit_link(UartBackend* u, uint8_t state) {
    u->link_state = state;
    BackendEvent e = {.type = BackendEvtLink};
    e.link.state = state;
    e.link.baud = u->baud;
    e.link.port_ok = u->serial != NULL;
    e.link.tx_bytes = u->tx_bytes;
    e.link.rx_bytes = u->rx_bytes;
    e.link.rx_dropped = u->rx_dropped;
    snprintf(e.link.version, sizeof(e.link.version), "%s", u->fw_version);
    emit(u, &e);
}

static void emit_error(UartBackend* u, uint8_t req, uint16_t code, const char* msg) {
    BackendEvent e = {.type = BackendEvtError};
    e.error.request = req;
    e.error.code = code;
    snprintf(e.error.msg, sizeof(e.error.msg), "%s", msg);
    emit(u, &e);
}

/* ---------------- serial ---------------- */

static void uart_rx_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UartBackend* u = context;
    if(event & FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(handle);
        /* a full buffer drops the byte (and with it a frame): counted for About */
        if(!furi_stream_buffer_send(u->rx, &b, 1, 0)) u->rx_dropped++;
    }
}

static void uart_write(void* ctx, const uint8_t* data, size_t len) {
    UartBackend* u = ctx;
    if(u->serial) {
        furi_hal_serial_tx(u->serial, data, len);
        u->tx_bytes += len;
        u->last_tx = u->now;
    }
}

/* ---------------- request queue (stop-and-wait below) ---------------- */

static bool enqueue(UartBackend* u, uint8_t type, const uint8_t* payload, size_t len) {
    if(u->link_state != BackendLinkUp && type != PROTO_T_HELLO) return false;
    if(u->q_count >= UART_QUEUE || len > PROTO_MAX_PAYLOAD) return false;
    QueuedReq* r = &u->q[(u->q_head + u->q_count) % UART_QUEUE];
    r->type = type;
    r->len = (uint16_t)len;
    r->payload = len ? malloc(len) : NULL;
    if(len) memcpy(r->payload, payload, len);
    u->q_count++;
    return true;
}

static void pump_queue(UartBackend* u) {
    if(proto_link_busy(&u->link) || !u->q_count) return;
    QueuedReq* r = &u->q[u->q_head];
    if(proto_link_send(&u->link, r->type, r->payload, r->len, u->now, NULL)) {
        /* the encoded frame now lives in the link; do not keep secrets in the queue */
        q_drop(r);
        u->q_head = (uint8_t)((u->q_head + 1) % UART_QUEUE);
        u->q_count--;
    }
}

static void send_hello(UartBackend* u) {
    ProtoHello h = {.proto_version = PROTO_VERSION, .max_baud = PROTO_BAUD_MAX};
    snprintf(h.version, sizeof(h.version), "ssh_runner %s", SSH_RUNNER_VERSION);
    uint8_t buf[64];
    size_t n = proto_enc_hello(&h, buf, sizeof(buf));
    if(!proto_link_busy(&u->link) &&
       proto_link_send(&u->link, PROTO_T_HELLO, buf, (uint16_t)n, u->now, NULL)) {
        u->hello_inflight = true;
        u->hello_at = u->now;
    }
}

/* ---------------- script streaming ---------------- */

static void pump_script(UartBackend* u) {
    if(u->script == ScriptIdle || u->q_count) return;
    uint8_t buf[PROTO_MAX_PAYLOAD];
    uint8_t t_chunk = u->script_is_key ? PROTO_T_KEY_BLOB_CHUNK : PROTO_T_SCRIPT_CHUNK;
    uint8_t t_end = u->script_is_key ? PROTO_T_KEY_BLOB_END : PROTO_T_SCRIPT_END;
    if(u->script == ScriptChunks) {
        uint8_t data[PROTO_SCRIPT_CHUNK];
        uint32_t want = u->script_total - u->script_off;
        if(want > sizeof(data)) want = sizeof(data);
        size_t n = want ? u->script_read(u->script_ctx, u->script_off, data, want) : 0;
        if(want && n == 0) {
            u->script = ScriptIdle;
            emit_error(
                u,
                t_chunk,
                ProtoErrInternal,
                u->script_is_key ? "Cannot read key file" : "Cannot read script");
            enqueue(u, PROTO_T_CANCEL, NULL, 0); /* also wipes a half-staged key */
            return;
        }
        if(n) {
            ProtoScriptChunk c = {.offset = u->script_off, .len = (uint16_t)n, .data = data};
            size_t len = proto_enc_script_chunk(&c, buf, sizeof(buf));
            if(enqueue(u, t_chunk, buf, len)) {
                u->script_crc = proto_crc32(data, n, u->script_crc);
                u->script_off += (uint32_t)n;
            }
        }
        /* a key file must not linger on the stack */
        memset(data, 0, sizeof(data));
        memset(buf, 0, sizeof(buf));
        if(u->script_off >= u->script_total) u->script = ScriptEnd;
    } else if(u->script == ScriptEnd) {
        ProtoScriptEnd e = {.crc32 = u->script_crc};
        size_t len = proto_enc_script_end(&e, buf, sizeof(buf));
        if(enqueue(u, t_end, buf, len)) {
            u->script = ScriptIdle;
            if(u->script_is_key && u->connect_pending) {
                size_t cn = proto_enc_ssh_connect(&u->connect_req, buf, sizeof(buf));
                if(cn && enqueue(u, PROTO_T_SSH_CONNECT, buf, cn)) u->connect_pending = false;
            }
            u->script_is_key = false;
        }
    }
}

/* ---------------- key files from the ESP32 ---------------- */

static void in_reset(UartBackend* u) {
    if(u->in_blob) {
        memset(u->in_blob, 0, u->in_total);
        free(u->in_blob);
        u->in_blob = NULL;
    }
    u->in_total = u->in_off = u->in_crc = 0;
}

/* KEY_BLOB_BEGIN / CHUNK / END events: a generated or exported private key. */
static bool on_key_blob(UartBackend* u, const ProtoFrame* f) {
    if(f->type == PROTO_T_KEY_BLOB_BEGIN) {
        ProtoKeyBlobBegin b;
        in_reset(u);
        if(!proto_dec_key_blob_begin(f->payload, f->len, &b) || b.total > BACKEND_KEY_IN_MAX)
            return false;
        u->in_blob = malloc(b.total);
        u->in_total = b.total;
        return u->in_blob != NULL;
    }
    if(f->type == PROTO_T_KEY_BLOB_CHUNK) {
        ProtoScriptChunk c;
        if(!u->in_blob || !proto_dec_script_chunk(f->payload, f->len, &c) ||
           c.offset != u->in_off || c.offset + c.len > u->in_total)
            return false;
        memcpy(u->in_blob + c.offset, c.data, c.len);
        u->in_off += c.len;
        u->in_crc = proto_crc32(c.data, c.len, u->in_crc);
        return true;
    }
    ProtoScriptEnd e;
    if(!u->in_blob || !proto_dec_script_end(f->payload, f->len, &e)) return false;
    bool ok = u->in_off == u->in_total && e.crc32 == u->in_crc;
    proto_link_ack(&u->link);
    if(ok) {
        BackendEvent ev = {.type = BackendEvtKeyBlob};
        ev.blob.data = u->in_blob;
        ev.blob.len = u->in_total;
        ev.blob.crc32 = u->in_crc;
        emit(u, &ev);
    } else {
        emit_error(u, PROTO_T_KEY_BLOB_END, ProtoErrBadPayload, "Key file damaged in transfer");
    }
    in_reset(u);
    return true; /* already acknowledged */
}

/* KEY_PUB_EVT pieces: emitted once the line is complete. */
static bool on_key_pub(UartBackend* u, const ProtoFrame* f) {
    ProtoKeyPub k;
    if(!proto_dec_key_pub(f->payload, f->len, &k)) return false;
    if(k.offset == 0) {
        free(u->in_pub);
        u->in_pub = malloc(k.total + 1u);
        u->in_pub_off = 0;
    }
    if(!u->in_pub || k.offset != u->in_pub_off) return false;
    memcpy(u->in_pub + k.offset, k.data, k.len);
    u->in_pub_off = (uint16_t)(u->in_pub_off + k.len);
    proto_link_ack(&u->link);
    if(u->in_pub_off >= k.total) {
        u->in_pub[k.total] = '\0';
        BackendEvent ev = {.type = BackendEvtKeyPub};
        ev.key.status = k.status;
        snprintf(ev.key.fp, sizeof(ev.key.fp), "%s", k.fp);
        ev.key.line = u->in_pub;
        ev.key.len = k.total;
        emit(u, &ev);
        free(u->in_pub);
        u->in_pub = NULL;
    }
    return true;
}

/* ---------------- incoming ---------------- */

static void on_frame(void* ctx, const ProtoFrame* f) {
    UartBackend* u = ctx;
    BackendEvent e;
    memset(&e, 0, sizeof(e));
    bool ok = false;
    switch(f->type) {
    case PROTO_T_NET_STATUS_EVT:
        e.type = BackendEvtNetStatus;
        ok = proto_dec_net_status(f->payload, f->len, &e.net);
        break;
    case PROTO_T_WIFI_SCAN_RESULT:
        e.type = BackendEvtScanResult;
        ok = proto_dec_scan_result(f->payload, f->len, &e.scan);
        break;
    case PROTO_T_PING_RESP:
        e.type = BackendEvtPing;
        ok = proto_dec_ping_resp(f->payload, f->len, &e.ping);
        break;
    case PROTO_T_WIFI_JOIN_RESULT:
        e.type = BackendEvtWifiJoinResult;
        ok = proto_dec_wifi_join_result(f->payload, f->len, &e.join);
        break;
    case PROTO_T_SSH_STATE_EVT:
        e.type = BackendEvtSshState;
        ok = proto_dec_ssh_state(f->payload, f->len, &e.ssh);
        break;
    case PROTO_T_HOSTKEY_PROMPT_EVT:
        e.type = BackendEvtHostkeyPrompt;
        ok = proto_dec_hostkey_prompt(f->payload, f->len, &e.hostkey);
        break;
    case PROTO_T_STREAM_DATA_EVT:
        e.type = BackendEvtStream;
        ok = proto_dec_stream_data(f->payload, f->len, &e.stream);
        break;
    case PROTO_T_EXEC_DONE_EVT:
        e.type = BackendEvtExecDone;
        ok = proto_dec_exec_done(f->payload, f->len, &e.done);
        break;
    case PROTO_T_KEY_BLOB_BEGIN:
    case PROTO_T_KEY_BLOB_CHUNK:
    case PROTO_T_KEY_BLOB_END:
        ok = on_key_blob(u, f);
        if(ok && f->type == PROTO_T_KEY_BLOB_END) return;
        if(ok) {
            proto_link_ack(&u->link);
            return;
        }
        in_reset(u);
        break;
    case PROTO_T_KEY_PUB_EVT:
        ok = on_key_pub(u, f);
        if(ok) return;
        break;
    case PROTO_T_SEC_STATE_EVT:
        e.type = BackendEvtSecState;
        ok = proto_dec_sec_state(f->payload, f->len, &e.sec_state);
        break;
    case PROTO_T_LOG_EVT: {
        ProtoLog l;
        if(proto_dec_log(f->payload, f->len, &l)) FURI_LOG_D(TAG, "esp32: %s", l.msg);
        proto_link_ack(&u->link);
        return;
    }
    default: {
        ProtoError err = {.code = ProtoErrUnknownType};
        uint8_t buf[80];
        size_t n = proto_enc_error(&err, buf, sizeof(buf));
        proto_link_reply(&u->link, PROTO_T_ERROR, buf, (uint16_t)n);
        return;
    }
    }
    if(!ok) {
        ProtoError err = {.code = ProtoErrBadPayload};
        uint8_t buf[80];
        size_t n = proto_enc_error(&err, buf, sizeof(buf));
        proto_link_reply(&u->link, PROTO_T_ERROR, buf, (uint16_t)n);
        return;
    }
    /* ACK first: the event may trigger new requests from the UI. */
    proto_link_ack(&u->link);
    emit(u, &e);
}

/* ---------------- link test ---------------- */

/* Frame k's payload: xorshift32 seeded from k, so every frame differs. */
static void lt_fill(uint32_t k, uint8_t* buf, size_t n) {
    uint32_t x = 0x9E3779B9u ^ (k * 2654435761u);
    if(!x) x = 1;
    for(size_t i = 0; i < n; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        buf[i] = (uint8_t)x;
    }
}

static void lt_emit(UartBackend* u) {
    BackendEvent e = {.type = BackendEvtLinkTest};
    u->lt.retransmits = u->link.stats.retransmits - u->lt_retx0;
    u->lt.ms = u->now - u->lt_start;
    u->lt.baud = u->baud;
    e.link_test = u->lt;
    emit(u, &e);
}

static void lt_pump(UartBackend* u) {
    if(!u->lt_active || proto_link_busy(&u->link) || u->q_count || u->switch_baud) return;
    if(u->lt.sent >= u->lt.frames) {
        lt_end(u);
        u->lt.done = true;
        lt_emit(u);
        return;
    }
    lt_fill(u->lt.sent, u->lt_buf, LT_LEN);
    proto_link_send(&u->link, PROTO_T_ECHO, u->lt_buf, LT_LEN, u->now, NULL);
}

static void on_reply(void* ctx, const ProtoFrame* f, uint8_t req) {
    UartBackend* u = ctx;
    BackendEvent e;
    memset(&e, 0, sizeof(e));
    if(f->type == PROTO_T_ERROR) {
        ProtoError err;
        if(!proto_dec_error(f->payload, f->len, &err)) memset(&err, 0, sizeof(err));
        if(req == PROTO_T_SCRIPT_BEGIN || req == PROTO_T_SCRIPT_CHUNK ||
           req == PROTO_T_SCRIPT_END || req == PROTO_T_KEY_BLOB_BEGIN ||
           req == PROTO_T_KEY_BLOB_CHUNK || req == PROTO_T_KEY_BLOB_END) {
            u->script = ScriptIdle;
            u->script_is_key = false;
            u->connect_pending = false; /* no SSH_CONNECT without its key */
        }
        emit_error(u, req, err.code, err.msg[0] ? err.msg : "Request refused");
        return;
    }
    switch(f->type) {
    case PROTO_T_ECHO_RESP:
        if(!u->lt_active) return;
        if(f->len != LT_LEN || memcmp(f->payload, u->lt_buf, f->len) != 0) u->lt.mismatches++;
        u->lt.sent++;
        u->lt.bytes += LT_LEN;
        if(u->lt.sent % 128 == 0) lt_emit(u);
        lt_pump(u); /* the link is free again inside on_reply */
        return;
    case PROTO_T_HELLO_RESP: {
        ProtoHelloResp r;
        u->hello_inflight = false;
        if(!proto_dec_hello_resp(f->payload, f->len, &r)) return;
        snprintf(u->fw_version, sizeof(u->fw_version), "%s", r.version);
        if(r.proto_version != PROTO_VERSION) {
            emit_link(u, BackendLinkMismatch);
            return;
        }
        if(r.baud != u->baud && r.baud >= PROTO_BAUD_INITIAL && r.baud <= PROTO_BAUD_MAX) {
            u->switch_baud = r.baud;
            u->switch_at = u->now + BAUD_SWITCH_MS;
        }
        if(u->link_state != BackendLinkUp) {
            emit_link(u, BackendLinkUp);
            enqueue(u, PROTO_T_NET_GET_STATUS, NULL, 0);
        }
        return;
    }
    case PROTO_T_NET_STATUS:
        e.type = BackendEvtNetStatus;
        if(proto_dec_net_status(f->payload, f->len, &e.net)) emit(u, &e);
        return;
    case PROTO_T_HOSTKEY_LIST_RESP:
        e.type = BackendEvtHostkeyList;
        if(proto_dec_hostkey_list(f->payload, f->len, &e.hostkeys)) emit(u, &e);
        return;
    case PROTO_T_SEC_STATUS:
        e.type = BackendEvtSecStatus;
        if(proto_dec_sec_status(f->payload, f->len, &e.sec)) emit(u, &e);
        return;
    case PROTO_T_ACK:
        if(req == PROTO_T_NET_SET_CFG || req == PROTO_T_HOSTKEY_DELETE ||
           req == PROTO_T_SECRET_FORGET) {
            e.type = BackendEvtOpDone;
            e.op_done = req;
            emit(u, &e);
        }
        return;
    default:
        return;
    }
}

static void on_timeout(void* ctx, uint8_t req, uint16_t seq) {
    UNUSED(seq);
    UartBackend* u = ctx;
    if(req == PROTO_T_HELLO) {
        u->hello_inflight = false;
        if(u->link_state != BackendLinkDown) emit_link(u, BackendLinkDown);
        /* a board may still be at the initial rate after a reset */
        if(u->baud != PROTO_BAUD_INITIAL && u->serial) {
            u->baud = PROTO_BAUD_INITIAL;
            furi_hal_serial_set_br(u->serial, u->baud);
        }
        return;
    }
    bool was_test = u->lt_active;
    if(u->lt_active) {
        lt_end(u);
        u->lt.aborted = true;
        u->lt.done = true;
        lt_emit(u);
    }
    /* A keepalive or link-test ECHO is internal: losing it is shown as "No board",
     * not as a failed user action. */
    if(req != PROTO_T_ECHO || was_test) {
        u->script = ScriptIdle;
        emit_error(u, req, BACKEND_ERR_TIMEOUT, "ESP32 not responding");
    }
    /* The board went away (unplugged, reset, or it fell back to the initial rate):
     * back to 115200 and re-handshake right away. */
    u->script = ScriptIdle;
    u->script_is_key = false;
    u->connect_pending = false;
    in_reset(u);
    for(unsigned i = 0; i < UART_QUEUE; i++)
        q_drop(&u->q[i]);
    u->q_count = 0;
    u->switch_baud = 0;
    proto_link_reset_peer(&u->link);
    if(u->baud != PROTO_BAUD_INITIAL && u->serial) {
        u->baud = PROTO_BAUD_INITIAL;
        furi_hal_serial_set_br(u->serial, u->baud);
    }
    emit_link(u, BackendLinkDown);
    u->hello_inflight = false;
    u->hello_at = u->now - HELLO_RETRY_MS;
}

/* ---------------- ops ---------------- */

static void uart_tick(void* self, uint32_t now) {
    UartBackend* u = self;
    u->now = now;
    uint8_t buf[128];
    size_t n;
    while((n = furi_stream_buffer_receive(u->rx, buf, sizeof(buf), 0)) > 0) {
        u->rx_bytes += n;
        proto_link_rx(&u->link, buf, n, now);
    }
    proto_link_poll(&u->link, now);

    if(u->switch_baud && (int32_t)(now - u->switch_at) >= 0 && !proto_link_busy(&u->link)) {
        u->baud = u->switch_baud;
        u->switch_baud = 0;
        furi_hal_serial_tx_wait_complete(u->serial);
        furi_hal_serial_set_br(u->serial, u->baud);
        send_hello(u); /* confirm at the new rate (PROTOCOL §4.1) */
    }
    if(u->link_state != BackendLinkUp && !u->hello_inflight &&
       now - u->hello_at >= HELLO_RETRY_MS) {
        send_hello(u);
        emit_link(u, u->link_state); /* refresh the About diagnostics */
    }

    pump_script(u);
    pump_queue(u);
    lt_pump(u);

    /* Keepalive: notices an unplugged or reset board, and keeps the ESP32 from
     * falling back to the initial rate (PROTOCOL §4.3). */
    if(u->link_state == BackendLinkUp && !proto_link_busy(&u->link) && !u->q_count &&
       !u->lt_active && u->script == ScriptIdle && !u->switch_baud &&
       now - u->last_tx >= KEEPALIVE_MS)
        proto_link_send(&u->link, PROTO_T_ECHO, NULL, 0, now, NULL);
}

static void uart_start(void* self) {
    UartBackend* u = self;
    emit_link(u, BackendLinkDown);
    if(!u->serial) {
        emit_error(u, PROTO_T_HELLO, ProtoErrBusy, "USART is in use");
        return;
    }
    send_hello(u);
}

static void uart_free(void* self) {
    UartBackend* u = self;
    if(u->serial) {
        if(u->link_state == BackendLinkUp) {
            /* best effort: end any SSH session before letting go of the port */
            uint8_t wire[PROTO_MAX_WIRE];
            size_t n = proto_frame_encode(
                PROTO_T_SSH_DISCONNECT, u->link.next_seq, NULL, 0, wire, sizeof(wire));
            furi_hal_serial_tx(u->serial, wire, n);
            furi_hal_serial_tx_wait_complete(u->serial);
        }
        furi_hal_serial_async_rx_stop(u->serial);
        furi_hal_serial_deinit(u->serial);
        furi_hal_serial_control_release(u->serial);
    }
    if(u->expansion) {
        expansion_enable(u->expansion);
        furi_record_close(RECORD_EXPANSION);
    }
    furi_stream_buffer_free(u->rx);
    in_reset(u);
    free(u->in_pub);
    for(unsigned i = 0; i < UART_QUEUE; i++)
        q_drop(&u->q[i]); /* queued payloads may hold key chunks or a passphrase */
    lt_end(u);
    memset(u, 0, sizeof(*u));
    free(u);
}

#define ENC_SEND(type, enc, msg)                  \
    do {                                          \
        uint8_t buf_[PROTO_MAX_PAYLOAD];          \
        size_t n_ = enc(msg, buf_, sizeof(buf_)); \
        if(n_ == 0) return false;                 \
        return enqueue(u, type, buf_, n_);        \
    } while(0)

static bool uart_net_get_status(void* self) {
    UartBackend* u = self;
    return enqueue(u, PROTO_T_NET_GET_STATUS, NULL, 0);
}
static bool uart_net_set_cfg(void* self, const ProtoNetCfg* c) {
    UartBackend* u = self;
    ENC_SEND(PROTO_T_NET_SET_CFG, proto_enc_net_cfg, c);
}
static bool uart_wifi_scan(void* self) {
    UartBackend* u = self;
    return enqueue(u, PROTO_T_WIFI_SCAN, NULL, 0);
}
static bool uart_wifi_join(void* self, const ProtoWifiJoin* j) {
    UartBackend* u = self;
    ENC_SEND(PROTO_T_WIFI_JOIN, proto_enc_wifi_join, j);
}
static bool uart_ping(void* self, const ProtoPingReq* r) {
    UartBackend* u = self;
    ENC_SEND(PROTO_T_PING_REQ, proto_enc_ping_req, r);
}
static bool uart_ssh_connect(void* self, const ProtoSshConnect* c) {
    UartBackend* u = self;
    ENC_SEND(PROTO_T_SSH_CONNECT, proto_enc_ssh_connect, c);
}
static bool uart_ssh_connect_key(
    void* self,
    const ProtoSshConnect* c,
    uint32_t total,
    BackendScriptRead read,
    void* ctx) {
    UartBackend* u = self;
    if(u->script != ScriptIdle || total == 0 || total > PROTO_KEY_MAX) return false;
    ProtoKeyBlobBegin b = {.total = total};
    uint8_t buf[16];
    size_t n = proto_enc_key_blob_begin(&b, buf, sizeof(buf));
    if(!n || !enqueue(u, PROTO_T_KEY_BLOB_BEGIN, buf, n)) return false;
    u->connect_req = *c;
    u->connect_pending = true;
    u->script = ScriptChunks;
    u->script_is_key = true;
    u->script_total = total;
    u->script_off = 0;
    u->script_crc = 0;
    u->script_read = read;
    u->script_ctx = ctx;
    return true;
}
static bool uart_key_passphrase(void* self, const ProtoKeyPassphrase* m) {
    UartBackend* u = self;
    uint8_t buf[PROTO_MAX_PAYLOAD];
    size_t n = proto_enc_key_passphrase(m, buf, sizeof(buf));
    bool ok = n && enqueue(u, PROTO_T_KEY_PASSPHRASE, buf, n);
    memset(buf, 0, sizeof(buf));
    return ok;
}
static bool uart_hostkey_decision(void* self, const ProtoHostkeyDecision* d) {
    UartBackend* u = self;
    ENC_SEND(PROTO_T_HOSTKEY_DECISION, proto_enc_hostkey_decision, d);
}
static bool uart_auth_password(void* self, const ProtoAuthPasswordMsg* m) {
    UartBackend* u = self;
    uint8_t buf[PROTO_MAX_PAYLOAD];
    size_t n = proto_enc_auth_password(m, buf, sizeof(buf));
    bool ok = n && enqueue(u, PROTO_T_AUTH_PASSWORD, buf, n);
    memset(buf, 0, sizeof(buf)); /* the queue copy is wiped once sent (pump_queue) */
    return ok;
}
static bool uart_exec(void* self, const char* command) {
    UartBackend* u = self;
    ProtoExecReq r = {.len = (uint16_t)strlen(command), .command = command};
    ENC_SEND(PROTO_T_EXEC_REQ, proto_enc_exec_req, &r);
}
static bool uart_run_script(
    void* self,
    const char* interp,
    uint32_t total,
    BackendScriptRead read,
    void* ctx) {
    UartBackend* u = self;
    if(u->script != ScriptIdle || total == 0 || total > PROTO_SCRIPT_MAX) return false;
    ProtoScriptBegin b = {.total = total};
    snprintf(b.interp, sizeof(b.interp), "%s", interp);
    uint8_t buf[64];
    size_t n = proto_enc_script_begin(&b, buf, sizeof(buf));
    if(!n || !enqueue(u, PROTO_T_SCRIPT_BEGIN, buf, n)) return false;
    u->script = ScriptChunks;
    u->script_total = total;
    u->script_off = 0;
    u->script_crc = 0;
    u->script_read = read;
    u->script_ctx = ctx;
    return true;
}
static bool uart_flow_credit(void* self, uint32_t bytes) {
    UartBackend* u = self;
    ProtoFlowCredit c = {.bytes = bytes};
    ENC_SEND(PROTO_T_FLOW_CREDIT, proto_enc_flow_credit, &c);
}
static bool uart_cancel(void* self) {
    UartBackend* u = self;
    u->script = ScriptIdle;
    u->script_is_key = false;
    u->connect_pending = false;
    return enqueue(u, PROTO_T_CANCEL, NULL, 0);
}
static bool uart_disconnect(void* self) {
    UartBackend* u = self;
    return enqueue(u, PROTO_T_SSH_DISCONNECT, NULL, 0);
}
static bool uart_key_gen(void* self, const ProtoKeyGen* g) {
    UartBackend* u = self;
    uint8_t buf[PROTO_MAX_PAYLOAD];
    size_t n = proto_enc_key_gen(g, buf, sizeof(buf));
    bool ok = n && enqueue(u, PROTO_T_KEY_GEN, buf, n);
    memset(buf, 0, sizeof(buf)); /* the passphrase */
    return ok;
}
static bool uart_hostkey_list(void* self, const char* profile) {
    UartBackend* u = self;
    ProtoProfileRef r;
    snprintf(r.profile, sizeof(r.profile), "%s", profile);
    ENC_SEND(PROTO_T_HOSTKEY_LIST, proto_enc_profile_ref, &r);
}
static bool uart_hostkey_delete(void* self, const char* profile, const char* fp) {
    UartBackend* u = self;
    ProtoHostkeyDelete d;
    snprintf(d.profile, sizeof(d.profile), "%s", profile);
    snprintf(d.fp, sizeof(d.fp), "%s", fp);
    ENC_SEND(PROTO_T_HOSTKEY_DELETE, proto_enc_hostkey_delete, &d);
}
static bool uart_secret_forget(void* self, uint8_t kind, const char* id) {
    UartBackend* u = self;
    ProtoSecretForget f = {.kind = kind};
    snprintf(f.id, sizeof(f.id), "%s", id);
    ENC_SEND(PROTO_T_SECRET_FORGET, proto_enc_secret_forget, &f);
}

static bool uart_sec_get_status(void* self) {
    UartBackend* u = self;
    return enqueue(u, PROTO_T_SEC_GET_STATUS, NULL, 0);
}
static bool uart_sec_enable(void* self, const char* word) {
    UartBackend* u = self;
    ProtoSecEnable m = {.tier = ProtoSecTierProtect};
    snprintf(m.word, sizeof(m.word), "%s", word);
    ENC_SEND(PROTO_T_SEC_ENABLE, proto_enc_sec_enable, &m);
}

static bool uart_link_test(void* self, uint32_t frames) {
    UartBackend* u = self;
    if(u->link_state != BackendLinkUp || u->lt_active || u->script != ScriptIdle || !frames)
        return false;
    memset(&u->lt, 0, sizeof(u->lt));
    u->lt.frames = frames;
    u->lt_start = u->now;
    u->lt_retx0 = u->link.stats.retransmits;
    u->lt_buf = malloc(LT_LEN);
    u->lt_active = true;
    lt_pump(u);
    return true;
}

static const BackendOps uart_ops = {
    .free = uart_free,
    .tick = uart_tick,
    .start = uart_start,
    .net_get_status = uart_net_get_status,
    .net_set_cfg = uart_net_set_cfg,
    .wifi_scan = uart_wifi_scan,
    .wifi_join = uart_wifi_join,
    .ping = uart_ping,
    .ssh_connect = uart_ssh_connect,
    .ssh_connect_key = uart_ssh_connect_key,
    .key_passphrase = uart_key_passphrase,
    .hostkey_decision = uart_hostkey_decision,
    .auth_password = uart_auth_password,
    .exec = uart_exec,
    .run_script = uart_run_script,
    .flow_credit = uart_flow_credit,
    .cancel = uart_cancel,
    .disconnect = uart_disconnect,
    .key_gen = uart_key_gen,
    .hostkey_list = uart_hostkey_list,
    .sec_get_status = uart_sec_get_status,
    .sec_enable = uart_sec_enable,
    .hostkey_delete = uart_hostkey_delete,
    .secret_forget = uart_secret_forget,
    .link_test = uart_link_test,
};

Backend uart_backend_create(BackendEventCallback cb, void* ctx) {
    UartBackend* u = malloc(sizeof(UartBackend));
    memset(u, 0, sizeof(*u));
    u->cb = cb;
    u->ctx = ctx;
    u->rx = furi_stream_buffer_alloc(RX_BUF, 1);
    u->baud = PROTO_BAUD_INITIAL;
    u->link_state = BackendLinkDown;
    u->hello_at = furi_get_tick();

    ProtoLinkCallbacks lcb = {
        .write = uart_write,
        .on_frame = on_frame,
        .on_reply = on_reply,
        .on_timeout = on_timeout,
        .ctx = u};
    proto_link_init(&u->link, &lcb);

    /* Expansion off before touching USART1, back on in uart_free(). */
    u->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(u->expansion);
    u->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(u->serial) {
        furi_hal_serial_init(u->serial, u->baud);
        furi_hal_serial_async_rx_start(u->serial, uart_rx_cb, u, false);
    } else {
        FURI_LOG_E(TAG, "USART1 busy");
    }
    Backend b = {.ops = &uart_ops, .self = u};
    return b;
}
