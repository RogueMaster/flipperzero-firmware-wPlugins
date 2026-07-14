#ifndef DARKFOREST_H
#define DARKFOREST_H

#include "globals.h"

#define Song const uint8_t PROGMEM

Song darkForest[] = {
    // total song in bytes = 114
    0x09, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x23,
    0x00, // Address of track 1 35
    0x29,
    0x00, // Address of track 2 41
    0x2C,
    0x00, // Address of track 3 44
    0x30,
    0x00, // Address of track 4 48
    0x37,
    0x00, // Address of track 5 55
    0x47,
    0x00, // Address of track 6 71
    0x4F,
    0x00, // Address of track 7 79
    0x56,
    0x00, // Address of track 8 86

    0x01, // Channel 0 entry track
    0x02, // Channel 1 entry track
    0x00, // Channel 2 entry track
    0x03, // Channel 3 entry track

    //"Track 0"
    0x9E,
    1,
    2,
    0,
    3, // FX: GOTO advanced: ch0 = 0x01 / ch1 = 0x02 / ch2 = 0x00 / ch3 = 0x03
    0xE0,
    127, // LONG DELAY: ticks = 65 + 127
    0x41,
    -6, // FX: VOLUME SLIDE ON: steps = -6
    0x40,
    16, // FX: SET VOLUME: volume = 16
    0xFC,
    8, // GOTO track 8
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0xFC,
    8, // GOTO track 8
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0xFC,
    8, // GOTO track 8
    0x4C,
    -1, // FX: SET TRANSPOSITION: notes = -1
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0xFC,
    8, // GOTO track 8
    0x40,
    16, // FX: SET VOLUME: volume = 16
    0xFC,
    8, // GOTO track 8
    0x00 + 59, // NOTE ON: note = 59
    0x9F + 4, // DELAY: ticks = 4
    0x4D, // FX: TRANSPOSITION OFF
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x9D,
    16, // SET song tempo: value = 16
    0xFD,
    3,
    4, // REPEAT: count = 3 + 1 / track = 4
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0xFC,
    5, // GOTO track 5
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 3"
    0xFD,
    3,
    7, // REPEAT: count = 3 + 1 / track = 7
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 4"
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    -1, // FX: VOLUME SLIDE ON: steps = -1
    0x00 + 2, // NOTE ON: note = 2
    0x9F + 64, // DELAY: ticks = 64
    0xFE, // RETURN

    //"Track 5"
    0xFC,
    6, // GOTO track 6
    0x4C,
    -4, // FX: SET TRANSPOSITION: notes = -4
    0xFC,
    6, // GOTO track 6
    0x4C,
    -5, // FX: SET TRANSPOSITION: notes = -5
    0xFC,
    6, // GOTO track 6
    0x4C,
    -7, // FX: SET TRANSPOSITION: notes = -7
    0xFC,
    6, // GOTO track 6
    0x4D, // FX: TRANSPOSITION OFF
    0xFE, // RETURN

    //"Track 6"
    0x9F + 16, // DELAY: ticks = 16
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0x41,
    -1, // FX: VOLUME SLIDE ON: steps = -1
    0x00 + 21, // NOTE ON: note = 21
    0x9F + 48, // DELAY: ticks = 48
    0xFE, // RETURN

    //"Track 7"
    0x9F + 48, // DELAY: ticks = 48
    0x41,
    -8, // FX: VOLUME SLIDE ON: steps = -8
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0x9F + 16, // DELAY: ticks = 16
    0xFE, // RETURN

    //"Track 8"
    0x00 + 59, // NOTE ON: note = 59
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 55, // NOTE ON: note = 55
    0x9F + 4, // DELAY: ticks = 4
    0xFE, // RETURN
};

#endif
