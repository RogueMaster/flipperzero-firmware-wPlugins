/*
 * TagTinker WiFi - Flipper <-> ESP32 framed UART protocol.
 *
 * This header is shared verbatim between the FAP and the ESP32 firmware so
 * frame layouts only need to be edited in one place.
 *
 * Frame on the wire:
 *
 *   +------+------+------+--------+----------+--------+
 *   | 0xAA | 0x55 | TYPE | LEN_LE | PAYLOAD  | CRC16  |
 *   +------+------+------+--------+----------+--------+
 *      1B     1B     1B     2B       LEN B      2B
 *
 * - TYPE: one of TT_FRAME_* below.
 * - LEN_LE: little-endian payload length (0..16383).
 * - CRC16: CRC-16/CCITT-FALSE over TYPE..end-of-PAYLOAD.
 *
 * Direction in comments: F->E = Flipper to ESP, E->F = ESP to Flipper.
 *
 * String encoding: zstrings are length-prefixed (u8 len, then bytes). NUL
 * terminator is *not* included on the wire. Empty strings are "\0\0".
 */
#ifndef TT_WIFI_PROTO_H
#define TT_WIFI_PROTO_H

#include <stdint.h>

#define TT_FRAME_SOF0 0xAAU
#define TT_FRAME_SOF1 0x55U

/* Maximum payload size we'll allocate buffers for on either side. Keep this
 * comfortably under the ESP's RX buffer; image data is chunked into
 * RESULT_CHUNK frames so this only bounds control traffic. */
#define TT_FRAME_MAX_PAYLOAD 1024U

/* Frame types -------------------------------------------------------------- */
enum {
    /* Handshake / status. */
    TT_FRAME_HELLO = 0x01, /* E->F: u16 fw_ver, u32 free_heap, zstring fw_name */
    TT_FRAME_PING = 0x02, /* F->E: empty   |  E->F: empty (echo) */

    /* WiFi config. */
    TT_FRAME_WIFI_SET = 0x10, /* F->E: zstring ssid, zstring password */
    TT_FRAME_WIFI_FORGET = 0x11, /* F->E: empty (clears NVS creds) */
    TT_FRAME_WIFI_STATUS = 0x12, /* either way:
                                    *   u8 state (TT_WIFI_*),
                                    *   i8 rssi,
                                    *   zstring ssid,
                                    *   zstring ip */

    /* Plugin discovery / execution. */
    TT_FRAME_LIST_PLUGINS = 0x20, /* F->E: empty */
    TT_FRAME_PLUGIN = 0x21, /* E->F: one frame per plugin (see below) */
    TT_FRAME_PLUGINS_END = 0x22, /* E->F: end-of-list sentinel */

    TT_FRAME_RUN_PLUGIN = 0x30, /* F->E: see TT_RUN_PLUGIN layout below */
    TT_FRAME_PROGRESS = 0x31, /* E->F: u8 percent, zstring message */
    TT_FRAME_RESULT_BEGIN = 0x32, /* E->F: u16 width, u16 height, u8 planes (1|2),
                                    *       u32 total_bytes */
    TT_FRAME_RESULT_CHUNK = 0x33, /* E->F: raw plane bytes */
    TT_FRAME_RESULT_END = 0x34, /* E->F: empty - all chunks delivered */
    TT_FRAME_ERROR = 0x3F, /* E->F: zstring message */
};

/* WiFi state codes (TT_FRAME_WIFI_STATUS payload byte 0). */
enum {
    TT_WIFI_DISCONNECTED = 0,
    TT_WIFI_CONNECTING = 1,
    TT_WIFI_CONNECTED = 2,
    TT_WIFI_AUTH_FAILED = 3,
    TT_WIFI_NO_AP = 4,
};

/*
 * TT_FRAME_PLUGIN payload layout
 * ------------------------------
 *   u8   plugin_index
 *   zstr id              (short stable id, e.g. "crypto")
 *   zstr name            (display name, e.g. "Crypto Price")
 *   zstr description     (one-liner shown on the run screen)
 *   u8   accent_modes    (bitmask: 1=mono, 2=red, 4=yellow)
 *   u8   param_count
 *   repeated param_count times:
 *     zstr key           (machine name, e.g. "symbol")
 *     zstr label         (display name, e.g. "Symbol")
 *     u8   type          (TT_PARAM_*)
 *     zstr default_value (always a string; client parses per type)
 *     if type == TT_PARAM_ENUM:
 *       u8   option_count
 *       repeated option_count times: zstr option
 *     if type == TT_PARAM_INT:
 *       i32_le min
 *       i32_le max
 */
enum {
    TT_PARAM_STRING = 0,
    TT_PARAM_INT = 1,
    TT_PARAM_ENUM = 2,
    TT_PARAM_BOOL = 3,
};

/* TT_FRAME_RUN_PLUGIN payload layout
 * ---------------------------------
 *   u8   plugin_index
 *   u16  target_w
 *   u16  target_h
 *   u8   accent       (TT_ACCENT_*)
 *   u8   param_count
 *   repeated: zstr key, zstr value (string-encoded, the plugin parses)
 */
enum {
    TT_ACCENT_NONE = 0, /* mono tag */
    TT_ACCENT_RED = 1,
    TT_ACCENT_YELLOW = 2,
};

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflect, no xor-out).
 * Tiny, branchless, no table - fine for the small frames we send. */
static inline uint16_t tt_crc16(const uint8_t* data, uint32_t len) {
    uint16_t crc = 0xFFFFU;
    for(uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(int b = 0; b < 8; b++) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

#endif /* TT_WIFI_PROTO_H */
