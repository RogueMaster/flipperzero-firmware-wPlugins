#ifndef FIELDSONG_H
#define FIELDSONG_H

#include "globals.h"

#define Song const uint8_t PROGMEM

Song fieldSong[] = {
    // total song in bytes = 53
    0x04, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x03,
    0x00, // Address of track 1 3
    0x12,
    0x00, // Address of track 2 17
    0x21,
    0x00, // Address of track 2 31

    0x01, // Channel 0 entry track
    0x00, // Channel 1 entry track
    0x00, // Channel 2 entry track
    0x00, // Channel 3 entry track

    //"Track 1"
    0x40,
    0, // FX: SET VOLUME: volume = 0
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 0"
    0x9D,
    36, // SET song tempo: value = 36
    0x9E,
    1,
    0,
    0,
    0, // FX: GOTO advanced: ch0 = 0x00 / ch1 = 0x01 / ch2 = 0x01 / ch3 = 0x01
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    -12, // FX: VOLUME SLIDE ON: steps = -12
    0xFD,
    63,
    2, // REPEAT: count = 63 + 1 / track = 2
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0xFD,
    3,
    3, // REPEAT: count = 3 + 1 / track = 3
    0x4C,
    -7, // FX: SET TRANSPOSITION: notes = -7
    0xFD,
    1,
    3, // REPEAT: count = 1 + 1 / track = 3
    0x4C,
    -2, // FX: SET TRANSPOSITION: notes = -2
    0xFD,
    1,
    3, // REPEAT: count = 1 + 1 / track = 3
    0x4D, // FX: TRANSPOSITION OFF
    0xFE, // RETURN

    //"Track 3"
    0x00 + 14, // NOTE ON: note = 14
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 14, // NOTE ON: note = 14
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 14, // NOTE ON: note = 14
    0x9F + 4, // DELAY: ticks = 4
    0xFE, // RETURN
};

#endif
