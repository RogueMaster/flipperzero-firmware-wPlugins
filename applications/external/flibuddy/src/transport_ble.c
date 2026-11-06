#include <furi.h>
#include <bt/bt_service/bt.h>

#include "ble_lines.h"
#include "ble_flibuddy_svc.h"
#include "ble_wire.h"
#include "line_sender.h"
#include "proto_encode.h"
#include "transport_ble.h"

#define TAG "FliBuddyBle"

#define BLE_WORKER_STACK 2048
#define BLE_RX_STREAM    1024 /* RX frames waiting for the worker */

/* The RX stream holds frames: a length byte and that many bytes written to
 * RX, or a lone BLE_RX_SESSION_END when the host unsubscribed or the link
 * went. The BLE event thread writes both, so the worker sees a session end
 * exactly between the old session's bytes and the new one's. */
#define BLE_RX_SESSION_END 0
#define BLE_RX_FRAME_MAX   BLE_LINES_CHUNK_MAX

_Static_assert(BLE_LINES_CHUNK_MAX <= LINE_SENDER_PACKET_MAX, "a chunk must fit the line sender");
_Static_assert(BLE_RX_FRAME_MAX <= UINT8_MAX, "a frame length must fit its byte");

typedef enum {
    BleEventStop = 1 << 0,
    BleEventRx = 1 << 1,
} BleEvent;

#define BLE_EVENTS_ALL (BleEventStop | BleEventRx)

typedef struct {
    Transport base;
    Bt* bt;
    FuriHalBleProfileBase* profile;
    FliBuddyBleCallbacks callbacks;
    FuriThread* worker;
    FuriStreamBuffer* rx_stream;
    uint8_t rx_frame[1 + BLE_RX_FRAME_MAX]; /* BLE event thread */
    bool rx_end_pending; /* BLE event thread: a session end not in the stream yet */
    uint8_t rx_bytes[BLE_RX_FRAME_MAX]; /* worker */
    FuriMutex* tx_lock; /* guards sender */
    LineSender sender;
    BleLines lines;
    char* line_storage;
    TransportRxCallback rx_callback;
    void* rx_ctx;
    volatile BtStatus status;
    volatile bool subscribed;
    bool opened;
} BleTransport;

static void worker_flags(BleTransport* t, uint32_t flags) {
    furi_thread_flags_set(furi_thread_get_id(t->worker), flags);
}

/* BLE event thread, the stream's only writer: a frame goes in whole or not
 * at all. `len` is 0 for a session end. */
static bool rx_frame(BleTransport* t, const uint8_t* data, size_t len) {
    if(furi_stream_buffer_spaces_available(t->rx_stream) < len + 1) return false;
    t->rx_frame[0] = (uint8_t)len;
    if(len > 0) memcpy(t->rx_frame + 1, data, len);
    furi_stream_buffer_send(t->rx_stream, t->rx_frame, len + 1, 0);
    return true;
}

/* False while a session end waits for room: bytes of the next session must
 * not overtake it. */
static bool rx_flush_end(BleTransport* t) {
    if(t->rx_end_pending && rx_frame(t, NULL, BLE_RX_SESSION_END)) t->rx_end_pending = false;
    return !t->rx_end_pending;
}

static void rx_session_end(BleTransport* t) {
    t->rx_end_pending = true;
    rx_flush_end(t);
    worker_flags(t, BleEventRx);
}

/* BLE event thread, through the service. */
static void svc_rx(const uint8_t* data, size_t len, void* ctx) {
    BleTransport* t = ctx;
    bool kept = rx_flush_end(t);
    while(kept && len > 0) {
        size_t n = len < BLE_RX_FRAME_MAX ? len : BLE_RX_FRAME_MAX;
        kept = rx_frame(t, data, n);
        data += n;
        len -= n;
    }
    if(!kept) FURI_LOG_W(TAG, "rx overflow");
    worker_flags(t, BleEventRx);
}

static void svc_subscribe(bool subscribed, void* ctx) {
    BleTransport* t = ctx;
    t->subscribed = subscribed;
    if(!subscribed) rx_session_end(t);
}

static void svc_mtu(uint16_t mtu, void* ctx) {
    BleTransport* t = ctx;
    ble_lines_set_mtu(&t->lines, mtu);
}

static void svc_disconnect(void* ctx) {
    BleTransport* t = ctx;
    t->subscribed = false;
    ble_lines_reset_chunk(&t->lines);
    rx_session_end(t);
}

/* Bt service thread. */
static void bt_status_changed(BtStatus status, void* ctx) {
    BleTransport* t = ctx;
    if(t) t->status = status;
}

static void on_line(const char* line, size_t len, void* ctx) {
    BleTransport* t = ctx;
    if(t->rx_callback) t->rx_callback(line, len, t->rx_ctx);
}

static int32_t ble_worker(void* ctx) {
    BleTransport* t = ctx;
    uint8_t len;
    for(;;) {
        uint32_t events = furi_thread_flags_wait(BLE_EVENTS_ALL, FuriFlagWaitAny, FuriWaitForever);
        if(events & FuriFlagError) continue;
        if(events & BleEventStop) break;
        /* frames are written whole, so the bytes after a length are there */
        while(furi_stream_buffer_receive(t->rx_stream, &len, 1, 0) == 1) {
            if(len == BLE_RX_SESSION_END) {
                /* a new host session never continues the old one's partial line */
                ble_lines_reset_rx(&t->lines);
            } else {
                furi_check(furi_stream_buffer_receive(t->rx_stream, t->rx_bytes, len, 0) == len);
                ble_lines_receive(&t->lines, t->rx_bytes, len);
            }
        }
    }
    return 0;
}

/* Sender thread, under tx_lock. */
static BleNotifyResult notify_chunk(const uint8_t* data, size_t len, void* ctx) {
    BleTransport* t = ctx;
    if(!t->subscribed) return BleNotifyLost;
    return flibuddy_ble_notify(t->profile, data, len) ? BleNotifyOk : BleNotifyBusy;
}

static uint32_t tick_ms(void* ctx) {
    UNUSED(ctx);
    return furi_get_tick(); /* 1 tick = 1 ms on f7 */
}

static bool ble_open(Transport* base) {
    BleTransport* t = (BleTransport*)base;
    if(t->opened) return true;

    t->bt = furi_record_open(RECORD_BT);
    t->status = BtStatusOff;
    t->subscribed = false;
    ble_lines_reset_chunk(&t->lines);
    ble_lines_reset_rx(&t->lines);
    furi_stream_buffer_reset(t->rx_stream);
    t->rx_end_pending = false;
    furi_thread_start(t->worker);
    bt_set_status_changed_callback(t->bt, bt_status_changed, t);

    t->profile = bt_profile_start(t->bt, flibuddy_ble_profile, &t->callbacks);
    if(!t->profile) {
        FURI_LOG_E(TAG, "profile not started");
        bt_profile_restore_default(t->bt);
        bt_set_status_changed_callback(t->bt, NULL, NULL);
        worker_flags(t, BleEventStop);
        furi_thread_join(t->worker);
        furi_record_close(RECORD_BT);
        t->bt = NULL;
        return false;
    }
    t->opened = true;
    return true;
}

static void ble_close(Transport* base) {
    BleTransport* t = (BleTransport*)base;
    if(!t->opened) return;

    t->opened = false;
    t->subscribed = false;
    bt_disconnect(t->bt);
    /* stops the profile, after which no service callback runs */
    if(!bt_profile_restore_default(t->bt)) FURI_LOG_E(TAG, "default profile not restored");
    t->profile = NULL;
    /* cleared last: status changes up to here find the transport alive */
    bt_set_status_changed_callback(t->bt, NULL, NULL);
    worker_flags(t, BleEventStop);
    furi_thread_join(t->worker);
    furi_record_close(RECORD_BT);
    t->bt = NULL;
    t->status = BtStatusOff;
}

/* Firmware 1.5.1 reports BtStatusConnected only after pairing, which a
 * host never does, so the subscription alone says a host is there; a
 * disconnect clears it (svc_disconnect). */
static BleWireLink ble_link(const BleTransport* t) {
    return ble_wire_link(t->opened, t->status == BtStatusAdvertising, t->subscribed);
}

static bool ble_is_connected(const Transport* base) {
    return ble_link((const BleTransport*)base) == BleWireLinkConnected;
}

static TransportSendResult ble_send_line(Transport* base, const char* line, size_t len) {
    BleTransport* t = (BleTransport*)base;
    if(!ble_is_connected(base) || len >= PROTO_LINE_MAX) return TransportNotSent;

    furi_mutex_acquire(t->tx_lock, FuriWaitForever);
    line_sender_set_packet_size(&t->sender, ble_lines_chunk(&t->lines));
    ble_lines_begin_line(&t->lines);
    TransportSendResult result = line_sender_send(&t->sender, line, len);
    furi_mutex_release(t->tx_lock);
    return result;
}

static void ble_set_rx_callback(Transport* base, TransportRxCallback callback, void* ctx) {
    BleTransport* t = (BleTransport*)base;
    furi_check(!t->opened);
    t->rx_callback = callback;
    t->rx_ctx = ctx;
}

Transport* transport_ble_alloc(void) {
    BleTransport* t = malloc(sizeof(BleTransport));
    memset(t, 0, sizeof(BleTransport));
    t->base.open = ble_open;
    t->base.close = ble_close;
    t->base.send_line = ble_send_line;
    t->base.set_rx_callback = ble_set_rx_callback;
    t->base.is_connected = ble_is_connected;
    t->callbacks = (FliBuddyBleCallbacks){
        .on_rx = svc_rx,
        .on_subscribe = svc_subscribe,
        .on_mtu = svc_mtu,
        .on_disconnect = svc_disconnect,
        .ctx = t,
    };
    t->status = BtStatusOff;
    t->line_storage = malloc(PROTO_LINE_MAX);
    ble_lines_init(
        &t->lines, t->line_storage, PROTO_LINE_MAX, on_line, t, notify_chunk, tick_ms, t);
    line_sender_init(&t->sender, ble_lines_chunk(&t->lines), ble_lines_send, &t->lines);
    t->rx_stream = furi_stream_buffer_alloc(BLE_RX_STREAM, 1);
    t->tx_lock = furi_mutex_alloc(FuriMutexTypeNormal);
    t->worker = furi_thread_alloc_ex("FliBuddyBle", BLE_WORKER_STACK, ble_worker, t);
    return &t->base;
}

void transport_ble_free(Transport* base) {
    BleTransport* t = (BleTransport*)base;
    ble_close(base);
    furi_thread_free(t->worker);
    furi_mutex_free(t->tx_lock);
    furi_stream_buffer_free(t->rx_stream);
    free(t->line_storage);
    free(t);
}

TransportBleStatus transport_ble_status(const Transport* base) {
    switch(ble_link((const BleTransport*)base)) {
    case BleWireLinkAdvertising:
        return TransportBleAdvertising;
    case BleWireLinkConnected:
        return TransportBleConnected;
    case BleWireLinkOff:
        break;
    }
    return TransportBleOff;
}
