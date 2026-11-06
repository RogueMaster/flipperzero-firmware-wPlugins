#include "ble_lines.h"

#define ATT_HEADER 3

void ble_lines_init(
    BleLines* bl,
    char* storage,
    size_t cap,
    LineBufferCallback on_line,
    void* line_ctx,
    BleLinesNotifyFn notify,
    BleLinesClockFn clock,
    void* ctx) {
    bl->chunk = BLE_LINES_CHUNK_MIN;
    bl->busy = 0;
    bl->notify = notify;
    bl->clock = clock;
    bl->ctx = ctx;
    bl->line_start = clock(ctx);
    line_buffer_init(&bl->lines, storage, cap, on_line, line_ctx);
}

void ble_lines_set_mtu(BleLines* bl, uint16_t mtu) {
    size_t chunk = mtu > ATT_HEADER ? (size_t)mtu - ATT_HEADER : 0;
    if(chunk < BLE_LINES_CHUNK_MIN) chunk = BLE_LINES_CHUNK_MIN;
    if(chunk > BLE_LINES_CHUNK_MAX) chunk = BLE_LINES_CHUNK_MAX;
    bl->chunk = (uint16_t)chunk;
}

void ble_lines_reset_chunk(BleLines* bl) {
    bl->chunk = BLE_LINES_CHUNK_MIN;
}

size_t ble_lines_chunk(const BleLines* bl) {
    return bl->chunk;
}

void ble_lines_begin_line(BleLines* bl) {
    bl->line_start = bl->clock(bl->ctx);
}

/* Hands one chunk to the stack, unless the line's time is up. */
static bool send_chunk(BleLines* bl, const uint8_t* data, size_t len) {
    if((uint32_t)(bl->clock(bl->ctx) - bl->line_start) >= BLE_LINES_LINE_BUDGET_MS) {
        bl->busy++;
        return false;
    }
    BleNotifyResult result = bl->notify(data, len, bl->ctx);
    if(result == BleNotifyBusy) bl->busy++;
    return result == BleNotifyOk;
}

LinePacketResult ble_lines_send(const uint8_t* data, size_t len, void* ctx) {
    BleLines* bl = ctx;
    size_t chunk = bl->chunk; /* one size for the whole packet */
    for(size_t pos = 0; pos < len; pos += chunk) {
        size_t n = len - pos < chunk ? len - pos : chunk;
        if(!send_chunk(bl, data + pos, n)) {
            return pos == 0 ? LinePacketNotSent : LinePacketMaybeSent;
        }
    }
    return LinePacketSent;
}

uint32_t ble_lines_busy(const BleLines* bl) {
    return bl->busy;
}

size_t ble_lines_receive(BleLines* bl, const uint8_t* data, size_t len) {
    return line_buffer_feed(&bl->lines, data, len);
}

void ble_lines_reset_rx(BleLines* bl) {
    line_buffer_reset(&bl->lines);
}
