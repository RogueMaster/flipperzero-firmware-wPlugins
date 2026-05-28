#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Seeed MR60FDA / R60AFD1 protocol parser.
 *
 * Frame layout:
 *   [0x53][0x59][CTRL][CMD][LEN_H][LEN_L][DATA...][CHKSUM][0x54][0x43]
 *
 * CHKSUM = sum(all bytes from header start through last data byte) & 0xFF
 *
 * Useful frames (sample subset — verify against the Seeed PDF for your batch):
 *   CTRL=0x80, CMD=0x01: human presence  (1 byte: 0/1)
 *   CTRL=0x80, CMD=0x02: motion info     (1 byte: 0=none, 1=static, 2=active)
 *   CTRL=0x83, CMD=0x01: fall alarm      (1 byte: 0/1)
 *   CTRL=0x83, CMD=0x04: residence time  (4 bytes BE, seconds)
 */

#define MR60_HDR0        0x53
#define MR60_HDR1        0x59
#define MR60_TAIL0       0x54
#define MR60_TAIL1       0x43
#define MR60_MAX_PAYLOAD 64

typedef enum {
    Mr60StHdr0,
    Mr60StHdr1,
    Mr60StCtrl,
    Mr60StCmd,
    Mr60StLenH,
    Mr60StLenL,
    Mr60StData,
    Mr60StChk,
    Mr60StTail0,
    Mr60StTail1,
} Mr60State;

typedef struct {
    Mr60State state;
    uint8_t   ctrl;
    uint8_t   cmd;
    uint16_t  length;
    uint16_t  data_idx;
    uint8_t   data[MR60_MAX_PAYLOAD];
    uint8_t   sum_acc;
} Mr60Parser;

// Decoded radar state — what the UI cares about.
typedef struct {
    bool     presence;
    uint8_t  motion;          // 0=none, 1=static, 2=active
    bool     fall_detected;
    uint32_t residence_sec;
    uint8_t  body_sign;       // 0x80/0x03 body movement energy (0..255)
    uint16_t distance_cm;     // 0x80/0x04 distance to person, cm
    int16_t  pos_x;           // 0x80/0x05 x position, cm (signed)
    int16_t  pos_y;           // 0x80/0x05 y position, cm (signed)
    // 0x80/0x0E height zone detection
    uint8_t  height[4];       // detection energy per 50cm zone
    uint16_t height_total;    // total detection count from 0x0E frame
    uint8_t  breath_rate;     // 0x81/0x02 breaths per minute
    uint8_t  heart_rate;      // 0x85/0x02 beats per minute
    uint32_t frames_total;
    uint32_t frames_bad;
    uint32_t last_frame_ms;   // furi_get_tick() of last good frame
} Mr60RadarState;

void mr60_parser_reset(Mr60Parser* p);

// Feed one byte. Returns true exactly when a valid frame has just completed —
// then read p->ctrl / p->cmd / p->length / p->data, and/or call mr60_apply().
bool mr60_parser_feed(Mr60Parser* p, uint8_t byte);

// Apply the just-completed frame (in *p) into the running state *s.
void mr60_apply(const Mr60Parser* p, Mr60RadarState* s);
