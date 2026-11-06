/* NDJSON over the FliBuddy GATT service, as a pure state machine.
 *
 * Outbound, ble_lines_send() is the packet function of a line_sender: it
 * cuts each packet into notifications of up to ATT MTU - 3 bytes (20 until
 * the MTU exchange says more, never more than BLE_LINES_CHUNK_MAX). One
 * notification is in flight at a time: the next chunk is handed to the
 * stack only once it took the current one. A busy stack (one that stayed
 * full while the SDK waited for room, about a second) or a lost link ends
 * the packet at once. The SDK waits for each chunk, so a stack that frees
 * room slowly but in time would hold the sender for the whole line: a line
 * therefore gets BLE_LINES_LINE_BUDGET_MS from ble_lines_begin_line(), and
 * no chunk goes out after that. A line holds the sender for its budget and
 * one SDK wait at most.
 *
 * Inbound, the bytes written to RX are framed into lines by a line_buffer.
 *
 * Threads: the chunk size may be set from the BLE event thread while a
 * sender reads it; everything else belongs to one thread per direction.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "line_buffer.h"
#include "line_sender.h"

#define BLE_LINES_CHUNK_MIN      20 /* the default ATT MTU 23 - 3 */
#define BLE_LINES_CHUNK_MAX      243 /* the characteristics' size */
/* A 64-block scan (~2.8 KB) is 16 chunks at the MTU Apple hosts agree on. */
#define BLE_LINES_LINE_BUDGET_MS 2000

typedef enum {
    BleNotifyOk, /* the stack took the chunk */
    BleNotifyBusy, /* no room in the stack, even after waiting */
    BleNotifyLost, /* no host subscribed to TX */
} BleNotifyResult;

typedef BleNotifyResult (*BleLinesNotifyFn)(const uint8_t* data, size_t len, void* ctx);

/* Milliseconds; may wrap. */
typedef uint32_t (*BleLinesClockFn)(void* ctx);

typedef struct {
    volatile uint16_t chunk;
    uint32_t busy; /* busy answers and chunks past a line's budget so far */
    BleLinesNotifyFn notify;
    BleLinesClockFn clock;
    void* ctx; /* of notify and clock */
    uint32_t line_start; /* clock at ble_lines_begin_line() */
    LineBuffer lines;
} BleLines;

/* `storage` of `cap` bytes holds the line being received (line_buffer.h). */
void ble_lines_init(
    BleLines* bl,
    char* storage,
    size_t cap,
    LineBufferCallback on_line,
    void* line_ctx,
    BleLinesNotifyFn notify,
    BleLinesClockFn clock,
    void* ctx);

/* The MTU the exchange agreed on; the chunk becomes mtu - 3 within
 * BLE_LINES_CHUNK_MIN..BLE_LINES_CHUNK_MAX. */
void ble_lines_set_mtu(BleLines* bl, uint16_t mtu);

/* The link is gone: the next one starts at BLE_LINES_CHUNK_MIN. */
void ble_lines_reset_chunk(BleLines* bl);

size_t ble_lines_chunk(const BleLines* bl);

/* A line starts: its time budget runs from now. */
void ble_lines_begin_line(BleLines* bl);

/* LineSenderPacketFn; `ctx` is the BleLines. A zero-length packet sends
 * nothing. NotSent when no chunk went out, MaybeSent when only some did. A
 * chunk past the line's budget counts as busy. */
LinePacketResult ble_lines_send(const uint8_t* data, size_t len, void* ctx);

uint32_t ble_lines_busy(const BleLines* bl);

/* Feeds bytes written to RX; returns the number of lines delivered. */
size_t ble_lines_receive(BleLines* bl, const uint8_t* data, size_t len);

/* A new host session: forgets a partial line. */
void ble_lines_reset_rx(BleLines* bl);
