/* ============================================================
 * ble_isp.c  —  BLE-Serial (BLE-UART) + STK500-Worker.
 *
 * Struktur 1:1 nach usb_isp.c — nur der Transport ist ausgetauscht:
 *   USB-CDC  ->  ble_profile_serial (Nordic-UART-artig).
 * Der komplette STK500<->SWD-Kern (stk500.c / lgt_swd.c) bleibt unberuehrt;
 * genau dafuer ist Stk500Io I/O-abstrahiert.
 *
 * Datenfluss:
 *   - RX: der Serial-Event-Callback (laeuft im BLE-Thread) schiebt die Bytes
 *     direkt in den Stream-Buffer und setzt EvtRx. Der Worker zieht sie via
 *     io_get() heraus. (Anders als bei USB, wo der Worker selbst per
 *     furi_hal_cdc_receive nachzieht — beim BLE liefert der Callback die Daten
 *     schon mit.)
 *   - TX: io_send() sendet in <=BLE_TX_MAX-Haeppchen und wartet je Haeppchen auf
 *     das DataSent-Event (Flusskontrolle, analog EvtTxComplete bei USB).
 *
 * BLE-Serial-API (bestaetigt gegen ufbt-SDK, F7, ble_glue-Struktur):
 *     profiles/serial_profile.h : ble_profile_serial, ble_profile_serial_tx,
 *                                 ble_profile_serial_set_event_callback
 *     services/serial_service.h : SerialServiceEvent{,Type}, ...Callback
 *     bt/bt_service/bt.h        : bt_profile_start / bt_profile_restore_default /
 *                                 bt_set_status_changed_callback
 * Manifest: requires=["bt"]. Falls bt_profile_* in deiner FW anders heisst, sind
 * die Aufrufe unten mit <<BLE>> markiert.
 * ============================================================ */
#include "ble_isp.h"
#include "stk500.h"

#include <string.h>

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>

/* <<BLE>> — Pfade fuer die ble_glue-SDK-Struktur (ufbt, F7).
 * serial_profile.h inkludiert services/serial_service.h selbst. */
#include <bt/bt_service/bt.h>
#include <profiles/serial_profile.h>

#define BLE_TX_MAX 200 /* Bytes pro Notification (Serial-Service kann ~244) */
#define RX_BUF     1024

#define EvtStop       (1UL << 0)
#define EvtRx         (1UL << 1)
#define EvtTxComplete (1UL << 2)

struct BleIsp {
    FuriThread* worker;
    FuriStreamBuffer* rx;
    Bt* bt;
    FuriHalBleProfileBase* profile;
    volatile bool run;
    volatile uint32_t activity;
    volatile uint32_t rx_bytes;
    volatile bool connected;
};

/* --- BLE-Serial-Event-Callback: laeuft im BLE-Thread ---
 * Rueckgabe = freier Pufferplatz (Flusskontrolle), damit uns der Stack nicht
 * ueberrollt. RX-Daten werden direkt in den Stream-Buffer geschoben. */
static uint16_t ble_event_cb(SerialServiceEvent event, void* ctx) {
    BleIsp* u = ctx;
    if(event.event == SerialServiceEventTypeDataReceived) {
        furi_stream_buffer_send(u->rx, event.data.buffer, event.data.size, 0);
        u->rx_bytes += event.data.size;
        furi_thread_flags_set(furi_thread_get_id(u->worker), EvtRx);
    } else if(event.event == SerialServiceEventTypeDataSent) {
        furi_thread_flags_set(furi_thread_get_id(u->worker), EvtTxComplete);
    }
    size_t space = furi_stream_buffer_spaces_available(u->rx);
    return space > 0xFFFF ? 0xFFFF : (uint16_t)space;
}

/* --- BT-Statuswechsel (verbunden / advertising) fuer die Anzeige --- */
static void ble_status_cb(BtStatus status, void* ctx) {
    BleIsp* u = ctx;
    u->connected = (status == BtStatusConnected);
    if(status == BtStatusConnected && u->profile) {
        /* Der Bt-Service reaktiviert beim Connect evtl. RPC (und/oder setzt einen
         * eigenen Serial-Callback). Darum den Kanal HIER erneut uebernehmen: */
        ble_profile_serial_set_event_callback(u->profile, RX_BUF, ble_event_cb, u); /* <<BLE>> */
        ble_profile_serial_set_rpc_active(u->profile, false); /* <<BLE>> */
    }
}

/* --- STK500-I/O (komplett im Worker-Thread) --- */
static bool io_get(void* ctx, uint8_t* b, uint32_t to) {
    BleIsp* u = ctx;
    if(furi_stream_buffer_is_empty(u->rx)) {
        uint32_t fl = furi_thread_flags_wait(EvtRx | EvtStop, FuriFlagWaitAny, to);
        if(fl & EvtStop) {
            u->run = false;
            return false;
        }
        /* bei EvtRx liegen die Daten bereits im Stream-Buffer (vom Callback) */
    }
    return furi_stream_buffer_receive(u->rx, b, 1, 0) == 1;
}
static void io_send(void* ctx, const uint8_t* buf, size_t n) {
    BleIsp* u = ctx;
    size_t off = 0;
    while(n && u->run) {
        size_t chunk = n > BLE_TX_MAX ? BLE_TX_MAX : n;
        furi_thread_flags_clear(EvtTxComplete);
        bool ok =
            ble_profile_serial_tx(u->profile, (uint8_t*)buf + off, (uint16_t)chunk); /* <<BLE>> */
        if(ok) {
            furi_thread_flags_wait(EvtTxComplete, FuriFlagWaitAny, 500); /* auf DataSent warten */
            off += chunk;
            n -= chunk;
        } else {
            furi_delay_ms(20); /* nicht verbunden / Puffer voll -> kurz zurueckhalten */
        }
    }
}
static void io_activity(void* ctx) {
    BleIsp* u = ctx;
    u->activity++;
}

/* --- BLE auf/zu: Serial-Profil starten, danach Standard-Profil zurueck --- */
static void ble_open(BleIsp* u) {
    u->bt = furi_record_open(RECORD_BT);
    bt_set_status_changed_callback(u->bt, ble_status_cb, u);
    u->profile = bt_profile_start(u->bt, ble_profile_serial, NULL); /* <<BLE>> */
    furi_hal_bt_start_advertising();
    ble_profile_serial_set_event_callback(u->profile, RX_BUF, ble_event_cb, u); /* <<BLE>> */
    ble_profile_serial_set_rpc_active(
        u->profile, false); /* <<BLE>> Kanal vom RPC uebernehmen -> Daten an unseren Callback */
}
static void ble_close(BleIsp* u) {
    bt_set_status_changed_callback(u->bt, NULL, NULL);
    bt_profile_restore_default(u->bt); /* <<BLE>> Standard-Profil (Mobile-App) zurueck */
    furi_record_close(RECORD_BT);
    u->bt = NULL;
    u->profile = NULL;
    u->connected = false;
}

static int32_t ble_worker(void* ctx) {
    BleIsp* u = ctx;
    ble_open(u);
    Stk500Io io = {
        .ctx = u,
        .get = io_get,
        .send = io_send,
        .run = &u->run,
        .activity = io_activity,
    };
    stk500_run(&io);
    ble_close(u);
    return 0;
}

BleIsp* ble_isp_start(void) {
    BleIsp* u = malloc(sizeof(BleIsp));
    memset(u, 0, sizeof(BleIsp));
    u->run = true;
    u->rx = furi_stream_buffer_alloc(RX_BUF, 1);
    u->worker = furi_thread_alloc_ex("LgtBleStk500", 2048, ble_worker, u);
    furi_thread_start(u->worker);
    return u;
}

void ble_isp_stop(BleIsp* u) {
    u->run = false;
    furi_thread_flags_set(furi_thread_get_id(u->worker), EvtStop);
    furi_thread_join(u->worker);
    furi_thread_free(u->worker);
    furi_stream_buffer_free(u->rx);
    free(u);
}

uint32_t ble_isp_activity(BleIsp* u) {
    return u->activity;
}
uint32_t ble_isp_rx_bytes(BleIsp* u) {
    return u->rx_bytes;
}
bool ble_isp_connected(BleIsp* u) {
    return u->connected;
}
