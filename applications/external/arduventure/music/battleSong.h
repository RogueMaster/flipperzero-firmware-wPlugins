#ifndef BATTLESONG_H
#define BATTLESONG_H

#include "globals.h"

#define Song const uint8_t PROGMEM

Song battleSong[] = {
    0x04, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x3E,
    0x00, // Address of track 1 //62
    0x3F,
    0x00, // Address of track 2 //63
    0x58,
    0x00, // Address of track 3 //88

    0x00, // Channel 0 entry track
    0x01, // Channel 1 entry track
    0x01, // Channel 2 entry track
    0x01, // Channel 3 entry track

    //"Track channel 0"
    0x9E,
    0,
    1,
    1,
    1, // FX: GOTO advanced: ch0 = 0x00 / ch1 = 0x01 / ch2 = 0x02 / ch3 = 0x03
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    -8, // FX: SLIDE VOLUME ON: -8
    0x50,
    5,
    1, // FX: SET VIBRATO: 5 1
    0x9D,
    24, // FX: SET TEMPO: tempo = 24
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    -2, // FX: SET TRANSPOSITION: -2
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    -2, // FX: SET TRANSPOSITION: -2
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFC,
    3, // GOTO track 3
    0x4C,
    -4, // FX: SET TRANSPOSITION: -4
    0xFC,
    3, // GOTO track 3
    0x4C,
    -2, // FX: SET TRANSPOSITION: -2
    0xFC,
    3, // GOTO track 3
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFD,
    1,
    3, // REPEAT: count = 1 + 1 / track = 3
    0x4C,
    -4, // FX: SET TRANSPOSITION: -4
    0xFC,
    3, // GOTO track 3
    0x4C,
    -2, // FX: SET TRANSPOSITION: -2
    0xFC,
    3, // GOTO track 3
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFC,
    3, // GOTO track 3
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track channel 1"
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track Track 1"
    0x00 + 25, // NOTE ON: note = 25
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 21, // NOTE ON: note = 21
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 20, // NOTE ON: note = 20
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 18, // NOTE ON: note = 18
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 20, // NOTE ON: note = 20
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 21, // NOTE ON: note = 21
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0xFE, // RETURN

    //"Track Track 2"
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 25, // NOTE ON: note = 25
    0x9F + 4, // DELAY: ticks = 4
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 4, // DELAY: ticks = 4
    0x00 + 25, // NOTE ON: note = 25
    0x9F + 12, // DELAY: ticks = 12
    0x9F + 4, // DELAY: ticks = 4
    0xFE, // RETURN
};

#endif
