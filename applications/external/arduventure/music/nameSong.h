#ifndef NAMESONG_H
#define NAMESONG_H

#include "globals.h"

#define Song const uint8_t PROGMEM

Song nameSong[] = {
    // total song in bytes = 49
    0x04, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x03,
    0x00, // Address of track 1
    0x0B,
    0x00, // Address of track 2
    0x1B,
    0x00, // Address of track 3

    0x00, // Channel 0 entry track
    0x00, // Channel 1 entry track
    0x01, // Channel 2 entry track
    0x00, // Channel 3 entry track

    //"Track 0"
    0x40,
    0, // FX: SET VOLUME: volume = 0
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x40,
    63, // set volume
    0x9D,
    16, // set tempo
    0xFD,
    63,
    2, // REPEAT: count = 63 + 1 / track = 2
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0x41,
    -8, // slide volume on
    0xFC,
    3, // GOTO track 3
    0x4C,
    -7, // FX: SET TRANSPOSITION: notes = -7
    0xFC,
    3, // GOTO track 3
    0x4C,
    -5, // FX: SET TRANSPOSITION: notes = -5
    0xFC,
    3, // GOTO track 3
    0x4D, // FX: TRANSPOSITION OFF
    0xFC,
    3, // GOTO track 3
    0xFE, // RETURN

    //"Track 3"
    0x00 + 49, // NOTE ON: note = 49
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 53, // NOTE ON: note = 53
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 56, // NOTE ON: note = 56
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 53, // NOTE ON: note = 53
    0x9F + 8, // DELAY: ticks = 8
    0xFE, // RETURN
};

#endif
