// Hotspot Arcade UART v2 wire protocol (ESP side).
// Must stay byte-for-byte identical to the Flipper side (flipper/.../ha_proto.h)
// and docs/PROTOCOL.md. Framed control messages + a raw-bulk escape for files.
#pragma once
#include <Arduino.h>

#define HA_UART_BAUD 921600
#define HA_SYNC 0xA5
#define HA_MAX_PAYLOAD 4096

// Firmware identity carried in every PING beacon: a 4-byte project MAGIC so a
// different project's beacon is never mistaken for ours, and a VERSION so the
// Flipper can flag an outdated board and offer to update it. Give each project
// its own MAGIC; bump VERSION whenever the protocol/features change.
#define HA_FW_MAGIC_0 0x48 // 'H'
#define HA_FW_MAGIC_1 0x41 // 'A'
#define HA_FW_MAGIC_2 0x52 // 'R'
#define HA_FW_MAGIC_3 0x43 // 'C'  ("HARC" = Hotspot ARCade)
#define HA_FW_VERSION 20 // v20: Frankendraw game + the ART artwork report

// Flipper -> ESP
enum {
    HA_MSG_CLEAR_FILES = 0x10,
    HA_MSG_FILE_BEGIN = 0x11, // hdr frame, then `total` raw bytes follow
    HA_MSG_SET_AP = 0x12,
    HA_MSG_START = 0x13,
    HA_MSG_STOP = 0x14,
    HA_MSG_RESET = 0x15,
    HA_MSG_SELECT_GAME = 0x16,
    HA_MSG_QUESTION = 0x17,
    HA_MSG_REVEAL = 0x18,
    HA_MSG_ROUND_END = 0x19,
    HA_MSG_CONFIG = 0x1A,
    HA_MSG_RESET_SCORES = 0x1B,
    HA_MSG_CONTENT_CLEAR = 0x1C, // drop all packs, for every game
    HA_MSG_CONTENT_PACK = 0x1D, // payload = game byte + pack name; begins a pack
    HA_MSG_CONTENT_ITEM = 0x1E, // payload = JSON object of the file's own keys
};

// ESP -> Flipper
enum {
    HA_MSG_STATUS = 0x80,
    HA_MSG_JOIN = 0x81,
    HA_MSG_LEAVE = 0x82,
    HA_MSG_SCORE = 0x83,
    HA_MSG_ROUND_RESULT = 0x84,
    HA_MSG_EVENT = 0x85,
    HA_MSG_PING = 0x86,
    HA_MSG_ART = 0x87, // finished artwork, streamed: op byte + JSON (see HA_ART_*)
};

// HA_MSG_ART op byte. A completed picture is streamed as BEGIN, one STROKE per line
// segment, then END, so neither side ever has to hold a whole drawing in RAM.
enum {
    HA_ART_BEGIN = 0, // {"game":..,"id":n,"w0":"A","w1":"B","w2":"C"} — opens a sheet
    HA_ART_STROKE = 1, // {"p":panel,"x0":..,"y0":..,"x1":..,"y1":..} in 0..255 sheet units
    HA_ART_END = 2, // {"id":n} — the sheet is complete
};

// Game ids
enum {
    HA_GAME_NONE = 0,
    HA_GAME_TRIVIA = 1,
    HA_GAME_CONNECT4 = 2,
    HA_GAME_TICTACTOE = 3,
    HA_GAME_DOTS = 4,
    HA_GAME_DRAW = 5,
    HA_GAME_PONG = 6,
    HA_GAME_REACT = 7, // reaction duel (fastest finger)
    HA_GAME_WYR = 8, // would you rather (poll)
    HA_GAME_SCRAMBLE = 9, // word scramble race
    HA_GAME_REVERSI = 10, // reversi/othello (duel kind)
    HA_GAME_GUESSCOLOR = 11, // guess the color (closest RGB + speed)
    HA_GAME_BATTLESHIP = 12, // battleship (1v1, hidden fleets)
    HA_GAME_SPECTRUM = 13, // wavelength-style spectrum guessing (party)
    HA_GAME_KMK = 14, // kiss marry kill (party, predict a player's picks)
    HA_GAME_CHESS = 15, // chess (1v1, full FIDE rules)
    HA_GAME_SECRETS = 16, // secrets (party, hidden yes/no vote + prediction)
    HA_GAME_FILLBLANK = 17, // fill the blank (party, judge picks the funniest answer)
    // 16 is Secrets (already on master) and 17 is reserved for a game in flight on
    // another branch; this game is trivially renumbered down to 17 if that one is
    // dropped or lands after it.
    HA_GAME_WEREWOLF = 18, // werewolf (party, hidden roles + night/day phases)
    HA_GAME_SPYFALL = 19, // spyfall (party, one player doesn't know the location)
    // 17..19 are claimed by games in flight on other branches (Werewolf took 19), so
    // this one starts at 20; the id renumbers trivially (it is not persisted anywhere).
    HA_GAME_FRANKENDRAW = 20, // "Draw a Monster": head/torso/legs by three hands (party)
};

// CRC-8/ATM: poly 0x07, init 0x00, no reflect, no xorout. Identical both sides.
static inline uint8_t ha_crc8_upd(uint8_t crc, uint8_t b) {
    crc ^= b;
    for(uint8_t i = 0; i < 8; i++) {
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

static inline uint8_t ha_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for(size_t i = 0; i < len; i++) crc = ha_crc8_upd(crc, data[i]);
    return crc;
}
