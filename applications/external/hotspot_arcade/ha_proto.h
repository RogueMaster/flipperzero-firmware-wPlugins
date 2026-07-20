#pragma once

#include <furi.h>
#include "ha_uart.h"

// Hotspot Arcade UART v2 wire protocol (Flipper side). Byte-for-byte identical to
// the ESP side (esp32/hotspot-arcade-fw/ha_proto.h) and docs/PROTOCOL.md.

#define HA_UART_BAUD   (921600)
#define HA_SYNC        (0xA5)
#define HA_MAX_PAYLOAD (4096)

// Firmware identity in the PING beacon (must match the ESP's ha_proto.h). MAGIC
// tells our board from another project's; VERSION lets us flag an outdated board.
#define HA_FW_MAGIC_0 0x48
#define HA_FW_MAGIC_1 0x41
#define HA_FW_MAGIC_2 0x52
#define HA_FW_MAGIC_3 0x43
#define HA_FW_VERSION 6 // the version this app requires; older board -> offer update

// Flipper -> ESP
#define HA_MSG_CLEAR_FILES  0x10
#define HA_MSG_FILE_BEGIN   0x11
#define HA_MSG_SET_AP       0x12
#define HA_MSG_START        0x13
#define HA_MSG_STOP         0x14
#define HA_MSG_RESET        0x15
#define HA_MSG_SELECT_GAME  0x16
#define HA_MSG_QUESTION     0x17
#define HA_MSG_REVEAL       0x18
#define HA_MSG_ROUND_END    0x19
#define HA_MSG_CONFIG       0x1A
#define HA_MSG_RESET_SCORES 0x1B
#define HA_MSG_TRIVIA_CLEAR 0x1C // drop stored topics (start of pack streaming)
#define HA_MSG_TRIVIA_TOPIC 0x1D // payload = topic name; creates a topic
#define HA_MSG_TRIVIA_Q     0x1E // payload = JSON {q,o[4],c}; appends to the last topic

// ESP -> Flipper
#define HA_MSG_STATUS       0x80
#define HA_MSG_JOIN         0x81
#define HA_MSG_LEAVE        0x82
#define HA_MSG_SCORE        0x83
#define HA_MSG_ROUND_RESULT 0x84
#define HA_MSG_EVENT        0x85
#define HA_MSG_PING         0x86

// Game ids
#define HA_GAME_NONE      0
#define HA_GAME_TRIVIA    1
#define HA_GAME_CONNECT4  2
#define HA_GAME_TICTACTOE 3
#define HA_GAME_DOTS      4
#define HA_GAME_DRAW      5
#define HA_GAME_PONG      6
#define HA_GAME_REACT     7 // reaction duel (fastest finger)
#define HA_GAME_WYR       8 // would you rather (poll)
#define HA_GAME_SCRAMBLE  9 // word scramble race
#define HA_GAME_REVERSI   10 // reversi/othello (duel kind)

static inline uint8_t ha_crc8_upd(uint8_t crc, uint8_t b) {
    crc ^= b;
    for(uint8_t i = 0; i < 8; i++) {
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

// Send a framed control message: SYNC | type | len(2 LE) | payload | crc8.
void ha_proto_send(HaUart* uart, uint8_t type, const uint8_t* payload, size_t len);

// Convenience: framed message with a NUL-terminated string payload.
void ha_proto_send_str(HaUart* uart, uint8_t type, const char* s);
