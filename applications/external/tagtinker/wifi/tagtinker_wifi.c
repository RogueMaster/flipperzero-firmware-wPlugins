/*
 * Flipper-side TagTinker WiFi link.
 *
 * Threading model:
 *
 *   - Open grabs the USART, starts the async RX, and spawns a worker thread.
 *   - The async RX ISR drops bytes into a stream_buffer.
 *   - The worker thread pulls bytes, runs the SOF/CRC parser, and either
 *     accumulates plugin manifests (which come in fragments) or directly
 *     dispatches simpler events to the user callback.
 *   - All callback invocations happen on the worker thread.
 *
 * The protocol matches esp32-wifi-fw/shared/tt_wifi_proto.h verbatim.
 */
#include "tagtinker_wifi.h"

#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>

#include <string.h>
#include <stdlib.h>

#define TAG  "TtWifi"
#define BAUD 230400U

struct TagTinkerWifi {
    FuriHalSerialHandle* serial;
    Expansion* expansion;
    FuriThread* worker;
    FuriStreamBuffer* rx_stream;
    volatile bool running;

    TtWifiEventCb cb;
    void* user;

    /* Reusable buffers. */
    TagTinkerWifiPlugin pending_plugin;
};

/* ---- Outgoing framing ---------------------------------------------------*/

static void emit(TagTinkerWifi* w, uint8_t type, const uint8_t* p, uint16_t len) {
    if(!w->serial) return;
    uint8_t hdr[5] = {
        TT_FRAME_SOF0, TT_FRAME_SOF1, type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    /* CRC over [type, len_lo, len_hi, payload]. */
    uint16_t crc = 0xFFFFU;
    auto inline void step(uint8_t b) {
        crc ^= (uint16_t)b << 8;
        for(int i = 0; i < 8; i++)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    for(int i = 2; i < 5; i++)
        step(hdr[i]);
    for(uint16_t i = 0; i < len; i++)
        step(p[i]);
    uint8_t tail[2] = {(uint8_t)(crc >> 8), (uint8_t)(crc & 0xFF)};

    furi_hal_serial_tx(w->serial, hdr, sizeof(hdr));
    if(len) furi_hal_serial_tx(w->serial, p, len);
    furi_hal_serial_tx(w->serial, tail, sizeof(tail));
    furi_hal_serial_tx_wait_complete(w->serial);
}

/* zstring writer helper. */
static uint16_t put_zstr(uint8_t* dst, uint16_t off, const char* s) {
    if(!s) s = "";
    size_t l = strlen(s);
    if(l > 255) l = 255;
    dst[off++] = (uint8_t)l;
    memcpy(dst + off, s, l);
    return (uint16_t)(off + l);
}

void tagtinker_wifi_ping(TagTinkerWifi* w) {
    emit(w, TT_FRAME_PING, NULL, 0);
}
void tagtinker_wifi_query_status(TagTinkerWifi* w) {
    emit(w, TT_FRAME_WIFI_STATUS, NULL, 0);
}
void tagtinker_wifi_list_plugins(TagTinkerWifi* w) {
    emit(w, TT_FRAME_LIST_PLUGINS, NULL, 0);
}
void tagtinker_wifi_forget(TagTinkerWifi* w) {
    emit(w, TT_FRAME_WIFI_FORGET, NULL, 0);
}

void tagtinker_wifi_set_creds(TagTinkerWifi* w, const char* ssid, const char* pwd) {
    uint8_t buf[160];
    uint16_t off = 0;
    off = put_zstr(buf, off, ssid);
    off = put_zstr(buf, off, pwd);
    emit(w, TT_FRAME_WIFI_SET, buf, off);
}

void tagtinker_wifi_run_plugin(
    TagTinkerWifi* w,
    uint8_t idx,
    uint16_t tw,
    uint16_t th,
    uint8_t accent,
    const TtWifiKV* kv,
    uint8_t n) {
    uint8_t buf[600];
    uint16_t off = 0;
    buf[off++] = idx;
    buf[off++] = (uint8_t)(tw & 0xFF);
    buf[off++] = (uint8_t)(tw >> 8);
    buf[off++] = (uint8_t)(th & 0xFF);
    buf[off++] = (uint8_t)(th >> 8);
    buf[off++] = accent;
    buf[off++] = n;
    for(uint8_t i = 0; i < n; i++) {
        off = put_zstr(buf, off, kv[i].key);
        off = put_zstr(buf, off, kv[i].value);
    }
    emit(w, TT_FRAME_RUN_PLUGIN, buf, off);
}

/* ---- Incoming parser ----------------------------------------------------*/

static bool rb_u8(const uint8_t* p, uint16_t len, uint16_t* pos, uint8_t* v) {
    if(*pos + 1 > len) return false;
    *v = p[(*pos)++];
    return true;
}
static bool rb_u16(const uint8_t* p, uint16_t len, uint16_t* pos, uint16_t* v) {
    uint8_t a, b;
    if(!rb_u8(p, len, pos, &a) || !rb_u8(p, len, pos, &b)) return false;
    *v = (uint16_t)a | ((uint16_t)b << 8);
    return true;
}
static bool rb_i32(const uint8_t* p, uint16_t len, uint16_t* pos, int32_t* v) {
    if(*pos + 4 > len) return false;
    uint32_t u = (uint32_t)p[*pos] | ((uint32_t)p[*pos + 1] << 8) | ((uint32_t)p[*pos + 2] << 16) |
                 ((uint32_t)p[*pos + 3] << 24);
    *pos += 4;
    *v = (int32_t)u;
    return true;
}
static bool rb_zstr(const uint8_t* p, uint16_t len, uint16_t* pos, char* out, size_t cap) {
    uint8_t l;
    if(!rb_u8(p, len, pos, &l)) return false;
    if(*pos + l > len) return false;
    size_t take = (l < cap - 1) ? l : cap - 1;
    memcpy(out, p + *pos, take);
    out[take] = 0;
    *pos += l;
    return true;
}

static void parse_plugin(TagTinkerWifi* w, const uint8_t* p, uint16_t len) {
    TagTinkerWifiPlugin* m = &w->pending_plugin;
    memset(m, 0, sizeof(*m));
    uint16_t pos = 0;
    if(!rb_u8(p, len, &pos, &m->index)) return;
    if(!rb_zstr(p, len, &pos, m->id, sizeof(m->id))) return;
    if(!rb_zstr(p, len, &pos, m->name, sizeof(m->name))) return;
    if(!rb_zstr(p, len, &pos, m->description, sizeof(m->description))) return;
    if(!rb_u8(p, len, &pos, &m->accent_modes)) return;
    if(!rb_u8(p, len, &pos, &m->param_count)) return;
    if(m->param_count > TT_WIFI_MAX_PARAMS) m->param_count = TT_WIFI_MAX_PARAMS;
    for(uint8_t i = 0; i < m->param_count; i++) {
        TtWifiParam* pp = &m->params[i];
        if(!rb_zstr(p, len, &pos, pp->key, sizeof(pp->key))) return;
        if(!rb_zstr(p, len, &pos, pp->label, sizeof(pp->label))) return;
        if(!rb_u8(p, len, &pos, &pp->type)) return;
        if(!rb_zstr(p, len, &pos, pp->default_value, sizeof(pp->default_value))) return;
        if(pp->type == TT_PARAM_ENUM) {
            if(!rb_u8(p, len, &pos, &pp->option_count)) return;
            if(pp->option_count > TT_WIFI_MAX_OPTIONS) pp->option_count = TT_WIFI_MAX_OPTIONS;
            for(uint8_t j = 0; j < pp->option_count; j++)
                if(!rb_zstr(p, len, &pos, pp->options[j], sizeof(pp->options[j]))) return;
        } else if(pp->type == TT_PARAM_INT) {
            if(!rb_i32(p, len, &pos, &pp->int_min)) return;
            if(!rb_i32(p, len, &pos, &pp->int_max)) return;
        }
    }
    TtWifiEvent ev = {.type = TtWifiEvtPlugin, .plugin = m};
    if(w->cb) w->cb(&ev, w->user);
}

static void dispatch(TagTinkerWifi* w, uint8_t type, const uint8_t* p, uint16_t len) {
    TtWifiEvent ev = {0};
    switch(type) {
    case TT_FRAME_HELLO: {
        uint16_t pos = 0;
        uint16_t fwver = 0;
        int32_t heap = 0;
        char name[32] = {0};
        rb_u16(p, len, &pos, &fwver);
        rb_i32(p, len, &pos, &heap);
        rb_zstr(p, len, &pos, name, sizeof(name));
        ev.type = TtWifiEvtHello;
        ev.u0 = fwver;
        ev.u1 = (uint32_t)heap;
        ev.str0 = name;
        if(w->cb) w->cb(&ev, w->user);
        break;
    }
    case TT_FRAME_WIFI_STATUS: {
        uint16_t pos = 0;
        uint8_t state = 0;
        uint8_t rssi_u = 0;
        char ssid[33] = {0}, ip[20] = {0};
        rb_u8(p, len, &pos, &state);
        rb_u8(p, len, &pos, &rssi_u);
        rb_zstr(p, len, &pos, ssid, sizeof(ssid));
        rb_zstr(p, len, &pos, ip, sizeof(ip));
        ev.type = TtWifiEvtWifiStatus;
        ev.u0 = state;
        ev.i1 = (int8_t)rssi_u;
        ev.str0 = ssid;
        ev.str1 = ip;
        if(w->cb) w->cb(&ev, w->user);
        break;
    }
    case TT_FRAME_PLUGIN:
        parse_plugin(w, p, len);
        break;
    case TT_FRAME_PLUGINS_END:
        ev.type = TtWifiEvtPluginsEnd;
        if(w->cb) w->cb(&ev, w->user);
        break;
    case TT_FRAME_PROGRESS: {
        uint16_t pos = 0;
        uint8_t pct = 0;
        char msg[80] = {0};
        rb_u8(p, len, &pos, &pct);
        rb_zstr(p, len, &pos, msg, sizeof(msg));
        ev.type = TtWifiEvtProgress;
        ev.u0 = pct;
        ev.str0 = msg;
        if(w->cb) w->cb(&ev, w->user);
        break;
    }
    case TT_FRAME_RESULT_BEGIN: {
        uint16_t pos = 0;
        uint16_t tw = 0, th = 0;
        uint8_t pl = 0;
        int32_t total = 0;
        rb_u16(p, len, &pos, &tw);
        rb_u16(p, len, &pos, &th);
        rb_u8(p, len, &pos, &pl);
        rb_i32(p, len, &pos, &total);
        ev.type = TtWifiEvtResultBegin;
        ev.u0 = ((uint32_t)th << 16) | tw;
        ev.u1 = pl;
        ev.u2 = (uint32_t)total;
        if(w->cb) w->cb(&ev, w->user);
        break;
    }
    case TT_FRAME_RESULT_CHUNK:
        ev.type = TtWifiEvtResultChunk;
        ev.data = p;
        ev.data_len = len;
        if(w->cb) w->cb(&ev, w->user);
        break;
    case TT_FRAME_RESULT_END:
        ev.type = TtWifiEvtResultEnd;
        if(w->cb) w->cb(&ev, w->user);
        break;
    case TT_FRAME_ERROR: {
        uint16_t pos = 0;
        char msg[100] = {0};
        rb_zstr(p, len, &pos, msg, sizeof(msg));
        ev.type = TtWifiEvtError;
        ev.str0 = msg;
        if(w->cb) w->cb(&ev, w->user);
        break;
    }
    default:
        break;
    }
}

/* ---- Worker (parser state machine) -------------------------------------*/

static int32_t worker_thread(void* ctx) {
    TagTinkerWifi* w = ctx;
    enum {
        S_SOF0,
        S_SOF1,
        S_TYPE,
        S_LEN_LO,
        S_LEN_HI,
        S_PAYLOAD,
        S_CRC_HI,
        S_CRC_LO
    } st = S_SOF0;
    uint8_t type = 0;
    uint16_t len = 0, idx = 0;
    uint16_t crc_calc = 0xFFFFU, crc_recv = 0;
    static uint8_t payload[TT_FRAME_MAX_PAYLOAD];
    uint32_t last_byte_tick = furi_get_tick();

    auto inline void step(uint8_t b) {
        crc_calc ^= (uint16_t)b << 8;
        for(int i = 0; i < 8; i++)
            crc_calc = (crc_calc & 0x8000U) ? (uint16_t)((crc_calc << 1) ^ 0x1021U) :
                                              (uint16_t)(crc_calc << 1);
    }

    /* Pull bytes in bulk from the stream buffer - one syscall per byte
     * was too slow at 230400 baud during plugin frame bursts and the ISR
     * was dropping bytes once the stream filled. */
    static uint8_t batch[256];

    while(w->running) {
        size_t got = furi_stream_buffer_receive(w->rx_stream, batch, sizeof(batch), 100);
        uint32_t now = furi_get_tick();
        if(got == 0) {
            if(now - last_byte_tick > furi_ms_to_ticks(3000)) {
                last_byte_tick = now;
                TtWifiEvent ev = {.type = TtWifiEvtLinkLost};
                if(w->cb) w->cb(&ev, w->user);
            }
            continue;
        }
        last_byte_tick = now;

        for(size_t k = 0; k < got; k++) {
            uint8_t b = batch[k];
            switch(st) {
            case S_SOF0:
                if(b == TT_FRAME_SOF0) st = S_SOF1;
                break;
            case S_SOF1:
                st = (b == TT_FRAME_SOF1) ? S_TYPE : S_SOF0;
                break;
            case S_TYPE:
                type = b;
                crc_calc = 0xFFFFU;
                step(b);
                st = S_LEN_LO;
                break;
            case S_LEN_LO:
                len = b;
                step(b);
                st = S_LEN_HI;
                break;
            case S_LEN_HI:
                len |= (uint16_t)b << 8;
                step(b);
                if(len > TT_FRAME_MAX_PAYLOAD) {
                    st = S_SOF0;
                    break;
                }
                idx = 0;
                st = (len == 0) ? S_CRC_HI : S_PAYLOAD;
                break;
            case S_PAYLOAD:
                payload[idx++] = b;
                step(b);
                if(idx >= len) st = S_CRC_HI;
                break;
            case S_CRC_HI:
                crc_recv = (uint16_t)b << 8;
                st = S_CRC_LO;
                break;
            case S_CRC_LO:
                crc_recv |= b;
                if(crc_recv == crc_calc) dispatch(w, type, payload, len);
                st = S_SOF0;
                break;
            }
        }
    }
    return 0;
}

static void rx_isr(FuriHalSerialHandle* h, FuriHalSerialRxEvent ev, void* ctx) {
    TagTinkerWifi* w = ctx;
    if(ev != FuriHalSerialRxEventData) return;
    /* Drain in chunks: one stream_buffer_send per byte was the dominant
     * cost in the ISR and could let the HAL FIFO overrun during the
     * plugin frame burst. */
    uint8_t buf[64];
    size_t n = 0;
    while(furi_hal_serial_async_rx_available(h)) {
        buf[n++] = furi_hal_serial_async_rx(h);
        if(n == sizeof(buf)) {
            furi_stream_buffer_send(w->rx_stream, buf, n, 0);
            n = 0;
        }
    }
    if(n) furi_stream_buffer_send(w->rx_stream, buf, n, 0);
}

/* ---- Lifecycle ---------------------------------------------------------- */

TagTinkerWifi* tagtinker_wifi_alloc(TtWifiEventCb cb, void* user) {
    TagTinkerWifi* w = malloc(sizeof(*w));
    memset(w, 0, sizeof(*w));
    w->cb = cb;
    w->user = user;
    /* 8 KB is plenty after the bulk-read ISR fix: the worker pulls 256
     * bytes at a time and never lags behind the burst from the ESP. The
     * older 16 KB sizing was a workaround for the per-byte syscall
     * bottleneck and just wastes heap that the IR TX pipeline could use. */
    w->rx_stream = furi_stream_buffer_alloc(8192, 1);
    return w;
}

void tagtinker_wifi_free(TagTinkerWifi* w) {
    if(!w) return;
    tagtinker_wifi_close(w);
    if(w->rx_stream) furi_stream_buffer_free(w->rx_stream);
    free(w);
}

bool tagtinker_wifi_open(TagTinkerWifi* w) {
    if(w->serial) return true;
    /* Yield the UART from the expansion service before grabbing it. */
    w->expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(w->expansion);

    w->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(!w->serial) {
        expansion_enable(w->expansion);
        furi_record_close(RECORD_EXPANSION);
        w->expansion = NULL;
        return false;
    }
    furi_hal_serial_init(w->serial, BAUD);

    w->running = true;
    w->worker = furi_thread_alloc_ex("TtWifiRx", 2048, worker_thread, w);
    furi_thread_start(w->worker);

    furi_hal_serial_async_rx_start(w->serial, rx_isr, w, false);
    return true;
}

void tagtinker_wifi_set_callback(
    TagTinkerWifi* w,
    TtWifiEventCb new_cb,
    void* new_user,
    TtWifiEventCb* out_prev_cb,
    void** out_prev_user) {
    if(out_prev_cb) *out_prev_cb = w->cb;
    if(out_prev_user) *out_prev_user = w->user;
    w->cb = new_cb;
    w->user = new_user;
}

void tagtinker_wifi_close(TagTinkerWifi* w) {
    if(!w->serial) return;
    furi_hal_serial_async_rx_stop(w->serial);
    w->running = false;
    if(w->worker) {
        furi_thread_join(w->worker);
        furi_thread_free(w->worker);
        w->worker = NULL;
    }
    furi_hal_serial_deinit(w->serial);
    furi_hal_serial_control_release(w->serial);
    w->serial = NULL;
    if(w->expansion) {
        expansion_enable(w->expansion);
        furi_record_close(RECORD_EXPANSION);
        w->expansion = NULL;
    }
}
