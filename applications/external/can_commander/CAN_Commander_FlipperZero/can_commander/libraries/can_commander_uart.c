#include "can_commander_uart.h"

#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#include <string.h>

#define CC_PACKET_RAW_MAX   (CC_MAX_PAYLOAD + 32U)
#define CC_PACKET_ENC_MAX   (CC_MAX_PAYLOAD + 40U)
#define CC_RX_STREAM_SIZE   2048U
#define CC_EVENT_QUEUE_LEN  48U
#define CC_WAIT_SLICE_MS    20U

typedef enum {
    CcKindCmd = 0x01,
    CcKindResp = 0x02,
    CcKindEvent = 0x03,
} CcPacketKind;

typedef enum {
    CcCmdPing = 0x01,
    CcCmdGetInfo = 0x02,
    CcCmdBusSetCfg = 0x10,
    CcCmdBusGetCfg = 0x11,
    CcCmdBusSetFilter = 0x12,
    CcCmdBusClearFilter = 0x13,
    CcCmdBusSendFrame = 0x14,
    CcCmdToolStart = 0x20,
    CcCmdToolStop = 0x21,
    CcCmdToolStatus = 0x22,
    CcCmdToolConfig = 0x23,
    CcCmdDbcClear = 0x30,
    CcCmdDbcAddSignal = 0x31,
    CcCmdDbcRemoveSignal = 0x32,
    CcCmdDbcList = 0x33,
    CcCmdStatsGet = 0x40,
} CcCommandId;

typedef enum {
    CcEventCanFrame = 0x80,
    CcEventTool = 0x81,
    CcEventDbcDecode = 0x82,
    CcEventStatus = 0x83,
    CcEventDrops = 0x84,
} CcEventId;

typedef struct {
    uint8_t ver;
    uint8_t kind;
    uint8_t id;
    uint8_t seq;
    uint16_t len;
    uint8_t payload[CC_MAX_PAYLOAD];
} CcPacket;

typedef struct {
    bool enabled;
    uint8_t cmd_id;
    uint8_t seq;
    bool matched;
    CcStatusCode status;
    uint8_t payload[CC_MAX_PAYLOAD];
    uint16_t payload_len;
} CcWaiter;

struct CcClient {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx_stream;
    FuriMutex* mutex;

    bool opened;
    uint8_t next_seq;

    uint8_t rx_encoded[CC_PACKET_ENC_MAX];
    uint16_t rx_encoded_len;

    CcEvent event_queue[CC_EVENT_QUEUE_LEN];
    uint16_t event_head;
    uint16_t event_count;
};

static inline uint16_t cc_read_u16_le(const uint8_t* src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

static inline uint32_t cc_read_u32_le(const uint8_t* src) {
    return ((uint32_t)src[0]) | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static inline void cc_write_u16_le(uint8_t* dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static inline void cc_write_u32_le(uint8_t* dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xFFU);
    dst[1] = (uint8_t)((value >> 8) & 0xFFU);
    dst[2] = (uint8_t)((value >> 16) & 0xFFU);
    dst[3] = (uint8_t)((value >> 24) & 0xFFU);
}

static inline void cc_write_float_le(uint8_t* dst, float value) {
    uint32_t raw = 0;
    memcpy(&raw, &value, sizeof(raw));
    cc_write_u32_le(dst, raw);
}

static inline bool cc_bus_is_valid(CcBus bus) {
    return bus == CcBusCan0 || bus == CcBusCan1 || bus == CcBusBoth;
}

static inline bool cc_lock(CcClient* client) {
    return furi_mutex_acquire(client->mutex, FuriWaitForever) == FuriStatusOk;
}

static inline void cc_unlock(CcClient* client) {
    furi_mutex_release(client->mutex);
}

static uint16_t cc_crc16_ccitt_false(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFFU;

    for(size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for(uint8_t bit = 0; bit < 8; bit++) {
            if(crc & 0x8000U) {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            } else {
                crc <<= 1U;
            }
        }
    }

    return crc;
}

static bool cc_cobs_encode(
    const uint8_t* input,
    uint16_t input_len,
    uint8_t* output,
    uint16_t output_max,
    uint16_t* output_len) {
    if(!input || !output || !output_len || output_max == 0) {
        return false;
    }

    uint16_t read_index = 0;
    uint16_t write_index = 1;
    uint16_t code_index = 0;
    uint8_t code = 1;

    while(read_index < input_len) {
        if(input[read_index] == 0) {
            if(code_index >= output_max) return false;
            output[code_index] = code;

            code = 1;
            code_index = write_index;
            write_index++;
            if(write_index > output_max) return false;

            read_index++;
        } else {
            if(write_index >= output_max) return false;
            output[write_index++] = input[read_index++];
            code++;

            if(code == 0xFFU) {
                if(code_index >= output_max) return false;
                output[code_index] = code;

                code = 1;
                code_index = write_index;
                write_index++;
                if(write_index > output_max) return false;
            }
        }
    }

    if(code_index >= output_max) return false;
    output[code_index] = code;
    *output_len = write_index;

    return true;
}

static bool cc_cobs_decode(
    const uint8_t* input,
    uint16_t input_len,
    uint8_t* output,
    uint16_t output_max,
    uint16_t* output_len) {
    if(!input || !output || !output_len || input_len == 0) {
        return false;
    }

    uint16_t read_index = 0;
    uint16_t write_index = 0;

    while(read_index < input_len) {
        const uint8_t code = input[read_index];
        if(code == 0) return false;

        read_index++;

        for(uint8_t i = 1; i < code; i++) {
            if(read_index >= input_len || write_index >= output_max) {
                return false;
            }
            output[write_index++] = input[read_index++];
        }

        if(code < 0xFFU && read_index < input_len) {
            if(write_index >= output_max) {
                return false;
            }
            output[write_index++] = 0;
        }
    }

    *output_len = write_index;

    return true;
}

static bool cc_parse_packet(const uint8_t* raw, uint16_t raw_len, CcPacket* out) {
    if(!raw || !out || raw_len < 8) {
        return false;
    }

    const uint16_t payload_len = cc_read_u16_le(&raw[4]);
    const uint16_t expected_len = (uint16_t)(6 + payload_len + 2);

    if(payload_len > CC_MAX_PAYLOAD || raw_len != expected_len) {
        return false;
    }

    const uint16_t packet_crc = cc_read_u16_le(&raw[6 + payload_len]);
    const uint16_t calc_crc = cc_crc16_ccitt_false(raw, (size_t)(6 + payload_len));

    if(packet_crc != calc_crc) {
        return false;
    }

    out->ver = raw[0];
    out->kind = raw[1];
    out->id = raw[2];
    out->seq = raw[3];
    out->len = payload_len;

    if(payload_len > 0) {
        memcpy(out->payload, &raw[6], payload_len);
    }

    return true;
}

static void cc_event_queue_clear(CcClient* client) {
    client->event_head = 0;
    client->event_count = 0;
}

static void cc_event_queue_push(CcClient* client, const CcEvent* event) {
    if(client->event_count == CC_EVENT_QUEUE_LEN) {
        client->event_head = (uint16_t)((client->event_head + 1U) % CC_EVENT_QUEUE_LEN);
        client->event_count--;
    }

    const uint16_t index =
        (uint16_t)((client->event_head + client->event_count) % CC_EVENT_QUEUE_LEN);
    client->event_queue[index] = *event;
    client->event_count++;
}

static bool cc_event_queue_pop(CcClient* client, CcEvent* out) {
    if(client->event_count == 0) {
        return false;
    }

    if(out) {
        *out = client->event_queue[client->event_head];
    }

    client->event_head = (uint16_t)((client->event_head + 1U) % CC_EVENT_QUEUE_LEN);
    client->event_count--;

    return true;
}

static bool cc_decode_event(const CcPacket* packet, CcEvent* out_event) {
    if(!packet || !out_event) {
        return false;
    }

    memset(out_event, 0, sizeof(*out_event));

    out_event->id = packet->id;
    out_event->raw_len = packet->len;

    switch(packet->id) {
    case CcEventCanFrame: {
        if(packet->len < 19) {
            out_event->type = CcEventTypeUnknown;
            return true;
        }

        out_event->type = CcEventTypeCanFrame;
        out_event->data.can_frame.ts_ms = cc_read_u32_le(&packet->payload[0]);
        out_event->data.can_frame.bus = (CcBus)packet->payload[4];
        out_event->data.can_frame.id = cc_read_u32_le(&packet->payload[5]);

        const uint8_t flags = packet->payload[9];
        out_event->data.can_frame.ext = (flags & 0x01U) != 0;
        out_event->data.can_frame.rtr = (flags & 0x02U) != 0;

        out_event->data.can_frame.dlc = packet->payload[10];
        if(out_event->data.can_frame.dlc > 8) {
            out_event->data.can_frame.dlc = 8;
        }

        memcpy(out_event->data.can_frame.data, &packet->payload[11], 8);
        return true;
    }

    case CcEventTool: {
        if(packet->len < 2) {
            out_event->type = CcEventTypeUnknown;
            return true;
        }

        out_event->type = CcEventTypeTool;
        out_event->data.tool.tool = (CcToolId)packet->payload[0];
        out_event->data.tool.code = (CcToolEventCode)packet->payload[1];
        out_event->data.tool.text_len = 0;

        const uint8_t* text_src = NULL;
        uint16_t text_len = 0;

        if(packet->len >= 3) {
            const uint8_t declared_len = packet->payload[2];
            const uint16_t available_with_declared_header = (uint16_t)(packet->len - 3U);

            if(declared_len <= available_with_declared_header) {
                text_src = &packet->payload[3];
                text_len = declared_len;
            } else {
                /* Fallback for legacy/alternate payloads that omit explicit text_len. */
                text_src = &packet->payload[2];
                text_len = (uint16_t)(packet->len - 2U);
            }
        }

        if(text_len > sizeof(out_event->data.tool.text) - 1U) {
            text_len = sizeof(out_event->data.tool.text) - 1U;
        }

        for(uint16_t i = 0; i < text_len; i++) {
            const uint8_t c = text_src[i];
            out_event->data.tool.text[i] = (c >= 32U && c <= 126U) ? (char)c : '.';
        }
        out_event->data.tool.text[text_len] = '\0';
        out_event->data.tool.text_len = (uint8_t)text_len;

        return true;
    }

    case CcEventDbcDecode: {
        if(packet->len < 32) {
            out_event->type = CcEventTypeUnknown;
            return true;
        }

        out_event->type = CcEventTypeDbcDecode;
        out_event->data.dbc_decode.sid = cc_read_u16_le(&packet->payload[0]);
        out_event->data.dbc_decode.bus = (CcBus)packet->payload[2];
        out_event->data.dbc_decode.frame_id = cc_read_u32_le(&packet->payload[3]);

        memcpy(&out_event->data.dbc_decode.raw, &packet->payload[7], sizeof(int64_t));
        memcpy(&out_event->data.dbc_decode.value, &packet->payload[15], sizeof(float));
        out_event->data.dbc_decode.in_range = packet->payload[19] != 0;

        memcpy(out_event->data.dbc_decode.unit, &packet->payload[20], CC_UNIT_TEXT_LEN);
        out_event->data.dbc_decode.unit[CC_UNIT_TEXT_LEN] = '\0';

        return true;
    }

    case CcEventStatus: {
        out_event->type = CcEventTypeStatus;
        uint16_t status_len = packet->len;
        if(status_len > CC_STATUS_PAYLOAD_MAX) {
            status_len = CC_STATUS_PAYLOAD_MAX;
        }
        out_event->data.status.len = status_len;
        if(status_len > 0) {
            memcpy(out_event->data.status.payload, packet->payload, status_len);
        }
        return true;
    }

    case CcEventDrops: {
        out_event->type = CcEventTypeDrops;

        if(packet->len >= 8) {
            out_event->data.drops.cli_dropped = cc_read_u32_le(&packet->payload[0]);
            out_event->data.drops.flipper_dropped = cc_read_u32_le(&packet->payload[4]);
        }

        return true;
    }

    default:
        out_event->type = CcEventTypeUnknown;
        return true;
    }
}

static void cc_handle_packet(CcClient* client, const CcPacket* packet, CcWaiter* waiter) {
    if(packet->ver != CC_PROTOCOL_VERSION) {
        return;
    }

    if(packet->kind == CcKindEvent) {
        CcEvent event = {0};
        if(cc_decode_event(packet, &event)) {
            cc_event_queue_push(client, &event);
        }
        return;
    }

    if(waiter && waiter->enabled && packet->kind == CcKindResp && packet->id == waiter->cmd_id &&
       packet->seq == waiter->seq) {
        waiter->matched = true;

        if(packet->len > 0) {
            waiter->status = (CcStatusCode)packet->payload[0];
            waiter->payload_len = packet->len - 1;
            if(waiter->payload_len > 0) {
                memcpy(waiter->payload, &packet->payload[1], waiter->payload_len);
            }
        } else {
            waiter->status = CcStatusUnknown;
            waiter->payload_len = 0;
        }
    }
}

static void cc_parse_bytes(CcClient* client, const uint8_t* data, size_t len, CcWaiter* waiter) {
    for(size_t i = 0; i < len; i++) {
        const uint8_t b = data[i];

        if(b == 0x00U) {
            if(client->rx_encoded_len == 0) {
                continue;
            }

            uint8_t decoded[CC_PACKET_RAW_MAX] = {0};
            uint16_t decoded_len = 0;

            if(cc_cobs_decode(
                   client->rx_encoded,
                   client->rx_encoded_len,
                   decoded,
                   sizeof(decoded),
                   &decoded_len)) {
                CcPacket packet = {0};
                if(cc_parse_packet(decoded, decoded_len, &packet)) {
                    cc_handle_packet(client, &packet, waiter);
                }
            }

            client->rx_encoded_len = 0;
            continue;
        }

        if(client->rx_encoded_len < sizeof(client->rx_encoded)) {
            client->rx_encoded[client->rx_encoded_len++] = b;
        } else {
            client->rx_encoded_len = 0;
        }
    }
}

static void cc_drain_stream_locked(CcClient* client, CcWaiter* waiter, uint32_t first_timeout_ms) {
    uint8_t buffer[64] = {0};
    bool first = true;

    while(true) {
        const uint32_t timeout = first ? first_timeout_ms : 0;
        first = false;

        const size_t rx =
            furi_stream_buffer_receive(client->rx_stream, buffer, sizeof(buffer), timeout);
        if(rx == 0) {
            break;
        }

        cc_parse_bytes(client, buffer, rx, waiter);

        if(waiter && waiter->matched) {
            break;
        }
    }
}

static bool cc_send_packet_locked(
    CcClient* client,
    uint8_t kind,
    uint8_t id,
    uint8_t seq,
    const uint8_t* payload,
    uint16_t payload_len) {
    if(!client->serial || payload_len > CC_MAX_PAYLOAD) {
        return false;
    }

    uint8_t raw[CC_PACKET_RAW_MAX] = {0};

    raw[0] = CC_PROTOCOL_VERSION;
    raw[1] = kind;
    raw[2] = id;
    raw[3] = seq;
    cc_write_u16_le(&raw[4], payload_len);

    if(payload_len > 0 && payload) {
        memcpy(&raw[6], payload, payload_len);
    }

    const uint16_t crc = cc_crc16_ccitt_false(raw, (size_t)(6 + payload_len));
    cc_write_u16_le(&raw[6 + payload_len], crc);

    uint8_t encoded[CC_PACKET_ENC_MAX] = {0};
    uint16_t encoded_len = 0;

    if(!cc_cobs_encode(
           raw, (uint16_t)(6 + payload_len + 2), encoded, sizeof(encoded), &encoded_len)) {
        return false;
    }

    furi_hal_serial_tx(client->serial, encoded, encoded_len);
    const uint8_t terminator = 0x00U;
    furi_hal_serial_tx(client->serial, &terminator, 1);
    furi_hal_serial_tx_wait_complete(client->serial);

    return true;
}

static bool cc_send_command_locked(
    CcClient* client,
    uint8_t cmd_id,
    const uint8_t* payload,
    uint16_t payload_len,
    CcStatusCode* out_status,
    uint8_t* out_payload,
    uint16_t* out_payload_len,
    uint32_t timeout_ms) {
    const uint8_t seq = ++client->next_seq;

    cc_drain_stream_locked(client, NULL, 0);

    if(!cc_send_packet_locked(client, CcKindCmd, cmd_id, seq, payload, payload_len)) {
        return false;
    }

    CcWaiter waiter = {
        .enabled = true,
        .cmd_id = cmd_id,
        .seq = seq,
        .matched = false,
        .status = CcStatusUnknown,
        .payload_len = 0,
    };

    const uint32_t started_at = furi_get_tick();

    while((uint32_t)(furi_get_tick() - started_at) < timeout_ms) {
        const uint32_t elapsed = (uint32_t)(furi_get_tick() - started_at);
        const uint32_t remaining = timeout_ms - elapsed;
        const uint32_t slice = remaining > CC_WAIT_SLICE_MS ? CC_WAIT_SLICE_MS : remaining;

        cc_drain_stream_locked(client, &waiter, slice);

        if(waiter.matched) {
            if(out_status) {
                *out_status = waiter.status;
            }
            if(out_payload_len) {
                *out_payload_len = waiter.payload_len;
            }
            if(out_payload && waiter.payload_len > 0) {
                memcpy(out_payload, waiter.payload, waiter.payload_len);
            }
            return true;
        }
    }

    return false;
}

static bool cc_exec(
    CcClient* client,
    uint8_t cmd_id,
    const uint8_t* payload,
    uint16_t payload_len,
    CcStatusCode* out_status,
    uint8_t* out_payload,
    uint16_t* out_payload_len,
    uint32_t timeout_ms) {
    if(!client || !cc_lock(client)) {
        return false;
    }

    bool ok = false;

    do {
        if(!client->opened || !client->serial) {
            break;
        }

        ok = cc_send_command_locked(
            client,
            cmd_id,
            payload,
            payload_len,
            out_status,
            out_payload,
            out_payload_len,
            timeout_ms);
    } while(false);

    cc_unlock(client);

    return ok;
}

static void cc_uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    if(!(event & FuriHalSerialRxEventData) || !context) {
        return;
    }

    CcClient* client = context;
    if(!client->rx_stream) {
        return;
    }

    while(furi_hal_serial_async_rx_available(handle)) {
        const uint8_t byte = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(client->rx_stream, &byte, sizeof(byte), 0);
    }
}

CcClient* cc_client_alloc(void) {
    CcClient* client = malloc(sizeof(CcClient));
    if(!client) {
        return NULL;
    }

    memset(client, 0, sizeof(CcClient));

    client->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!client->mutex) {
        free(client);
        return NULL;
    }

    client->rx_stream = furi_stream_buffer_alloc(CC_RX_STREAM_SIZE, 1);
    if(!client->rx_stream) {
        furi_mutex_free(client->mutex);
        free(client);
        return NULL;
    }

    return client;
}

void cc_client_close(CcClient* client) {
    if(!client || !cc_lock(client)) {
        return;
    }

    if(client->opened && client->serial) {
        furi_hal_serial_async_rx_stop(client->serial);
        furi_hal_serial_deinit(client->serial);
        furi_hal_serial_control_release(client->serial);
    }

    client->serial = NULL;
    client->opened = false;
    client->next_seq = 0;
    client->rx_encoded_len = 0;
    cc_event_queue_clear(client);

    if(client->rx_stream) {
        furi_stream_buffer_reset(client->rx_stream);
    }

    cc_unlock(client);
}

void cc_client_free(CcClient* client) {
    if(!client) {
        return;
    }

    cc_client_close(client);

    if(client->rx_stream) {
        furi_stream_buffer_free(client->rx_stream);
    }

    if(client->mutex) {
        furi_mutex_free(client->mutex);
    }

    free(client);
}

bool cc_client_open(CcClient* client) {
    if(!client || !cc_lock(client)) {
        return false;
    }

    bool ok = false;

    do {
        if(client->opened) {
            ok = true;
            break;
        }

        client->serial = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
        if(!client->serial) {
            break;
        }

        if(!furi_hal_serial_is_baud_rate_supported(client->serial, CC_UART_BAUD)) {
            furi_hal_serial_control_release(client->serial);
            client->serial = NULL;
            break;
        }

        furi_hal_serial_init(client->serial, CC_UART_BAUD);
        furi_hal_serial_async_rx_start(client->serial, cc_uart_rx_callback, client, false);

        client->opened = true;
        client->next_seq = 0;
        client->rx_encoded_len = 0;
        cc_event_queue_clear(client);
        furi_stream_buffer_reset(client->rx_stream);

        ok = true;
    } while(false);

    cc_unlock(client);

    return ok;
}

bool cc_client_is_open(const CcClient* client) {
    if(!client) {
        return false;
    }

    return client->opened;
}

bool cc_client_poll(CcClient* client, uint32_t timeout_ms) {
    if(!client || !cc_lock(client)) {
        return false;
    }

    bool ok = false;

    do {
        if(!client->opened || !client->rx_stream) {
            break;
        }

        cc_drain_stream_locked(client, NULL, timeout_ms);
        ok = true;
    } while(false);

    cc_unlock(client);

    return ok;
}

bool cc_client_pop_event(CcClient* client, CcEvent* out_event) {
    if(!client || !cc_lock(client)) {
        return false;
    }

    const bool ok = cc_event_queue_pop(client, out_event);
    cc_unlock(client);
    return ok;
}

bool cc_client_ping(CcClient* client, CcStatusCode* out_status) {
    return cc_exec(client, CcCmdPing, NULL, 0, out_status, NULL, NULL, 300);
}

bool cc_client_get_info(CcClient* client, CcStatusCode* out_status) {
    return cc_exec(client, CcCmdGetInfo, NULL, 0, out_status, NULL, NULL, 300);
}

bool cc_client_bus_set_cfg(
    CcClient* client,
    CcBus bus,
    uint32_t bitrate,
    bool listen_only,
    CcStatusCode* out_status) {
    if(!cc_bus_is_valid(bus)) {
        return false;
    }

    uint8_t payload[6] = {0};
    payload[0] = (uint8_t)bus;
    cc_write_u32_le(&payload[1], bitrate);
    payload[5] = listen_only ? 1U : 0U;

    return cc_exec(client, CcCmdBusSetCfg, payload, sizeof(payload), out_status, NULL, NULL, 400);
}

bool cc_client_bus_get_cfg(CcClient* client, CcBus bus, CcStatusCode* out_status) {
    if(!cc_bus_is_valid(bus)) {
        return false;
    }

    const uint8_t payload[1] = {(uint8_t)bus};
    return cc_exec(client, CcCmdBusGetCfg, payload, sizeof(payload), out_status, NULL, NULL, 300);
}

bool cc_client_bus_set_filter(
    CcClient* client,
    CcBus bus,
    uint32_t mask,
    uint32_t filter,
    bool ext_match,
    bool ext,
    CcStatusCode* out_status) {
    if(bus != CcBusCan0 && bus != CcBusCan1) {
        return false;
    }

    uint8_t payload[11] = {0};
    payload[0] = (uint8_t)bus;
    cc_write_u32_le(&payload[1], mask);
    cc_write_u32_le(&payload[5], filter);
    payload[9] = ext_match ? 1U : 0U;
    payload[10] = ext ? 1U : 0U;

    return cc_exec(
        client,
        CcCmdBusSetFilter,
        payload,
        sizeof(payload),
        out_status,
        NULL,
        NULL,
        300);
}

bool cc_client_bus_clear_filter(CcClient* client, CcBus bus, CcStatusCode* out_status) {
    if(bus != CcBusCan0 && bus != CcBusCan1) {
        return false;
    }

    const uint8_t payload[1] = {(uint8_t)bus};
    return cc_exec(
        client,
        CcCmdBusClearFilter,
        payload,
        sizeof(payload),
        out_status,
        NULL,
        NULL,
        300);
}

bool cc_client_send_frame(
    CcClient* client,
    CcBus bus,
    uint32_t id,
    bool ext,
    bool rtr,
    uint8_t dlc,
    const uint8_t data[8],
    CcStatusCode* out_status) {
    if((bus != CcBusCan0 && bus != CcBusCan1) || dlc > 8 || !data) {
        return false;
    }

    uint8_t payload[15] = {0};
    payload[0] = (uint8_t)bus;
    cc_write_u32_le(&payload[1], id);
    payload[5] = (ext ? 0x01U : 0x00U) | (rtr ? 0x02U : 0x00U);
    payload[6] = dlc;
    memcpy(&payload[7], data, 8);

    return cc_exec(
        client,
        CcCmdBusSendFrame,
        payload,
        sizeof(payload),
        out_status,
        NULL,
        NULL,
        300);
}

bool cc_client_tool_start(
    CcClient* client,
    CcToolId tool_id,
    const char* args,
    CcStatusCode* out_status) {
    if(tool_id == CcToolNone) {
        return false;
    }

    const uint16_t args_len = args ? (uint16_t)strnlen(args, CC_MAX_PAYLOAD - 1U) : 0;

    if(args && args[args_len] != '\0') {
        return false;
    }

    uint8_t payload[CC_MAX_PAYLOAD] = {0};
    payload[0] = (uint8_t)tool_id;
    if(args_len > 0) {
        memcpy(&payload[1], args, args_len);
    }

    return cc_exec(
        client,
        CcCmdToolStart,
        payload,
        (uint16_t)(1 + args_len),
        out_status,
        NULL,
        NULL,
        400);
}

bool cc_client_tool_stop(CcClient* client, CcStatusCode* out_status) {
    return cc_exec(client, CcCmdToolStop, NULL, 0, out_status, NULL, NULL, 300);
}

bool cc_client_tool_status(CcClient* client, CcStatusCode* out_status) {
    return cc_exec(client, CcCmdToolStatus, NULL, 0, out_status, NULL, NULL, 300);
}

bool cc_client_tool_config(CcClient* client, const char* args, CcStatusCode* out_status) {
    const uint16_t args_len = args ? (uint16_t)strnlen(args, CC_MAX_PAYLOAD) : 0;
    if(args && args[args_len] != '\0') {
        return false;
    }

    return cc_exec(
        client,
        CcCmdToolConfig,
        (const uint8_t*)args,
        args_len,
        out_status,
        NULL,
        NULL,
        300);
}

bool cc_client_dbc_clear(CcClient* client, CcStatusCode* out_status) {
    return cc_exec(client, CcCmdDbcClear, NULL, 0, out_status, NULL, NULL, 300);
}

bool cc_client_dbc_add_signal(
    CcClient* client,
    const CcDbcSignalDef* def,
    CcStatusCode* out_status) {
    if(!def || !cc_bus_is_valid(def->bus) || def->bit_len == 0) {
        return false;
    }

    uint8_t payload[40] = {0};
    payload[0] = (uint8_t)def->bus;
    cc_write_u32_le(&payload[1], def->id);
    payload[5] = def->ext ? 1U : 0U;
    payload[6] = def->start_bit;
    payload[7] = def->bit_len;
    payload[8] = def->motorola_order ? 1U : 0U;
    payload[9] = def->signed_value ? 1U : 0U;
    cc_write_float_le(&payload[10], def->factor);
    cc_write_float_le(&payload[14], def->offset);
    cc_write_float_le(&payload[18], def->min);
    cc_write_float_le(&payload[22], def->max);

    memset(&payload[26], 0, CC_UNIT_TEXT_LEN);
    if(def->unit[0]) {
        memcpy(&payload[26], def->unit, strnlen(def->unit, CC_UNIT_TEXT_LEN));
    }

    cc_write_u16_le(&payload[38], def->sid);

    return cc_exec(
        client,
        CcCmdDbcAddSignal,
        payload,
        sizeof(payload),
        out_status,
        NULL,
        NULL,
        400);
}

bool cc_client_dbc_remove_signal(CcClient* client, uint16_t sid, CcStatusCode* out_status) {
    uint8_t payload[2] = {0};
    cc_write_u16_le(payload, sid);

    return cc_exec(
        client,
        CcCmdDbcRemoveSignal,
        payload,
        sizeof(payload),
        out_status,
        NULL,
        NULL,
        300);
}

bool cc_client_dbc_list(CcClient* client, CcStatusCode* out_status) {
    return cc_exec(client, CcCmdDbcList, NULL, 0, out_status, NULL, NULL, 300);
}

bool cc_client_stats_get(CcClient* client, CcStatusCode* out_status) {
    return cc_exec(client, CcCmdStatsGet, NULL, 0, out_status, NULL, NULL, 300);
}

const char* cc_status_to_string(CcStatusCode status) {
    switch(status) {
    case CcStatusOk:
        return "OK";
    case CcStatusBadCmd:
        return "BAD_CMD";
    case CcStatusBadArg:
        return "BAD_ARG";
    case CcStatusBusErr:
        return "BUS_ERR";
    case CcStatusLocked:
        return "LOCKED";
    case CcStatusNoMem:
        return "NO_MEM";
    case CcStatusCrcFail:
        return "CRC_FAIL";
    case CcStatusNotActive:
        return "NOT_ACTIVE";
    default:
        return "UNKNOWN";
    }
}

const char* cc_bus_to_string(CcBus bus) {
    switch(bus) {
    case CcBusCan0:
        return "can0";
    case CcBusCan1:
        return "can1";
    case CcBusBoth:
        return "both";
    default:
        return "?";
    }
}

const char* cc_tool_to_string(CcToolId tool) {
    switch(tool) {
    case CcToolReadAll:
        return "read_all";
    case CcToolFiltered:
        return "filtered";
    case CcToolWrite:
        return "write";
    case CcToolSpeed:
        return "speed";
    case CcToolValtrack:
        return "valtrack";
    case CcToolUniqueIds:
        return "unique_ids";
    case CcToolBittrack:
        return "bittrack";
    case CcToolReverse:
        return "byte_watcher";
    case CcToolObdPid:
        return "obd_pid";
    case CcToolDbcDecode:
        return "dbc_decode";
    case CcToolCustomInject:
        return "custom_inject";
    default:
        return "none";
    }
}
