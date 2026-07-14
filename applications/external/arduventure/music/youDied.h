#ifndef SONGS_H
#define SONGS_H

#include "globals.h"

#define Song const uint8_t PROGMEM

Song youDied[] = {
    // total song in bytes = 64
    0x06, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x0A,
    0x00, // Address of track 1
    0x12,
    0x00, // Address of track 2
    0x15,
    0x00, // Address of track 3
    0x20,
    0x00, // Address of track 4
    0x2C,
    0x00, // Address of track 5

    0x01, // Channel 0 entry track
    0x02, // Channel 1 entry track
    0x00, // Channel 2 entry track
    0x02, // Channel 3 entry track

    //"Track 0"
    0x40,
    80, // FX: SET VOLUME: volume = 80
    0x9D,
    14, // SET song tempo: value = 14
    0x41,
    -6, // FX: VOLUME SLIDE ON: steps = -6
    0xFD,
    63,
    3, // REPEAT: count = 63 + 1 / track = 3
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    -1, // FX: VOLUME SLIDE ON: steps = -1
    0xFD,
    20,
    4, // REPEAT: count = 20 + 1 / track = 4
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0x40,
    0, // FX: SET VOLUME: volume = 0
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 3"
    0x00 + 43, // NOTE ON: note = 31
    0x9F + 16, // DELAY: ticks = 32
    0x00 + 50, // NOTE ON: note = 38
    0x9F + 16, // DELAY: ticks = 32
    0x00 + 46, // NOTE ON: note = 34
    0x9F + 16, // DELAY: ticks = 32
    0x00 + 46, // NOTE ON: note = 34
    0x9F + 8, // DELAY: ticks = 16
    0x00 + 45, // NOTE ON: note = 33
    0x9F + 8, // DELAY: ticks = 16
    0xFE, // RETURN

    //"Track 4"
    0xFC,
    5, // GOTO track 5
    0x4C,
    -2, // FX: SET TRANSPOSITION: notes = -2
    0xFC,
    5, // GOTO track 5
    0x4C,
    2, // FX: SET TRANSPOSITION: notes = 2
    0xFC,
    5, // GOTO track 5
    0x4D, // FX: TRANSPOSITION OFF
    0xFE, // RETURN

    //"Track 5"
    0x00 + 5, // NOTE ON: note = 5
    0x9F + 64, // DELAY: ticks = 64
    0xFE, // RETURN
};

#endif
