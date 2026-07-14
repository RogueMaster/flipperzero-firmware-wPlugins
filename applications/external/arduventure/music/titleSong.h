#ifndef TITLESONG_H
#define TITLESONG_H

#include "globals.h"

#define Song const uint8_t PROGMEM

Song titleSong[] = {
    // total song in bytes = 184
    0x12, // Number of tracks
    0x00,
    0x00, // Address of track 0     //0
    0x03,
    0x00, // Address of track 1     //3
    0x0B,
    0x00, // Address of track 2     //11
    0x1C,
    0x00, // Address of track 3     //28
    0x23,
    0x00, // Address of track 4     //35
    0x2A,
    0x00, // Address of track 5     //42
    0x31,
    0x00, // Address of track 6     //49
    0x38,
    0x00, // Address of track 7     //56
    0x4A,
    0x00, // Address of track 8     //74
    0x4C,
    0x00, // Address of track 9     //76
    0x5F,
    0x00, // Address of track 10    //95
    0x62,
    0x00, // Address of track 11    //98
    0x65,
    0x00, // Address of track 12    //101
    0x68,
    0x00, // Address of track 13    //104
    0x6B,
    0x00, // Address of track 14    //107
    0x72,
    0x00, // Address of track 15    //114
    0x83,
    0x00, // Address of track 16    //131
    0x89,
    0x00, // Address of track 17    //137

    0x07, // Channel 0 entry track (PULSE)
    0x01, // Channel 1 entry track (SQUARE)
    0x00, // Channel 2 entry track (TRIANGLE)
    0x0E, // Channel 3 entry track (NOISE)

    //"Track 0"
    0x40,
    0, // FX: SET VOLUME: volume = 0
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x9D,
    48, // SET song tempo: value = 48
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0xFD,
    15,
    2, // REPEAT: count = 15 + 1 / track = 2
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0x41,
    -2, // FX: VOLUME SLIDE ON: steps = -2
    0x9C,
    2, // ADD song tempo: value = 2
    0xFD,
    3,
    3, // REPEAT: count = 3 + 1 / track = 3
    0x9C,
    2, // ADD song tempo: value = 2
    0xFD,
    3,
    4, // REPEAT: count = 3 + 1 / track = 4
    0x9C,
    2, // ADD song tempo: value = 2
    0xFD,
    3,
    5, // REPEAT: count = 3 + 1 / track = 5
    0x9C,
    2, // ADD song tempo: value = 2
    0xFD,
    3,
    6, // REPEAT: count = 3 + 1 / track = 6
    0xFE, // RETURN

    //"Track 3"
    0x00 + 37, // NOTE ON: note = 37
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 40, // NOTE ON: note = 40
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 44, // NOTE ON: note = 44
    0x9F + 16, // DELAY: ticks = 16
    0xFE, // RETURN

    //"Track 4"
    0x00 + 37, // NOTE ON: note = 37
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 40, // NOTE ON: note = 40
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 45, // NOTE ON: note = 45
    0x9F + 16, // DELAY: ticks = 16
    0xFE, // RETURN

    //"Track 5"
    0x00 + 37, // NOTE ON: note = 37
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 42, // NOTE ON: note = 42
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 46, // NOTE ON: note = 46
    0x9F + 16, // DELAY: ticks = 16
    0xFE, // RETURN

    //"Track 6"
    0x00 + 37, // NOTE ON: note = 37
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 42, // NOTE ON: note = 42
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 47, // NOTE ON: note = 47
    0x9F + 16, // DELAY: ticks = 16
    0xFE, // RETURN

    //"Track 7"
    0x40,
    0, // FX: SET VOLUME: volume = 0
    0xFD,
    15,
    8, // REPEAT: count = 15 + 1 / track = 8
    0xFD,
    14,
    9, // REPEAT: count = 14 + 1 / track = 9
    0x55, // FX: NOTE CUT OFF
    0x9D,
    32, // SET song tempo: value = 32
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    -1, // FX: SLIDE VOLUME ON: -1
    0xFC,
    10, // GOTO: track = 10
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 8"
    0x9F + 48, // DELAY: ticks = 48
    0xFE, // RETURN

    //"Track 9"
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0x41,
    1, // FX: SLIDE VOLUME ON: 1
    0x54,
    3, // FX: SET NOTE CUT: 3
    0xFD,
    3,
    10, // REPEAT: count = 3 + 1 / track = 10
    0xFD,
    3,
    11, // REPEAT: count = 3 + 1 / track = 11
    0xFD,
    3,
    12, // REPEAT: count = 3 + 1 / track = 12
    0xFD,
    3,
    13, // REPEAT: count = 3 + 1 / track = 13
    0xFE, // RETURN

    //"Track 10"
    0x00 + 1, // NOTE ON: note = 1
    0x9F + 48, // DELAY: ticks = 48
    0xFE, // RETURN

    //"Track 11"
    0x00 + 4, // NOTE ON: note = 4
    0x9F + 48, // DELAY: ticks = 48
    0xFE, // RETURN

    //"Track 12"
    0x00 + 6, // NOTE ON: note = 6
    0x9F + 48, // DELAY: ticks = 48
    0xFE, // RETURN

    //"Track 13"
    0x00 + 11, // NOTE ON: note = 11
    0x9F + 48, // DELAY: ticks = 48
    0xFE, // RETURN

    //"Track 14"
    0xFD,
    31,
    8, // REPEAT: count = 31 + 1 / track = 8
    0xFD,
    55,
    15, // REPEAT: count = 55 + 1 / track = 15
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 15"
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0xFC,
    16, // GOTO track 16
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0xFC,
    16, // GOTO track 16
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0xFC,
    17, // GOTO track 17
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0xFC,
    16, // GOTO track 16
    0xFE, // RETURN

    //"Track 16"
    0x41,
    -8, // FX: VOLUME SLIDE ON: steps = -8
    0x9F + 6, // DELAY: ticks = 6
    0x43, // FX: VOLUME SLIDE OFF
    0x9F + 42, // DELAY: ticks = 42
    0xFE, // RETURN

    //"Track 17"
    0x41,
    -1, // FX: VOLUME SLIDE ON: steps = -1
    0x9F + 32, // DELAY: ticks = 32
    0x43, // FX: VOLUME SLIDE OFF
    0x9F + 16, // DELAY: ticks = 16
    0xFE, // RETURN
};

#endif
