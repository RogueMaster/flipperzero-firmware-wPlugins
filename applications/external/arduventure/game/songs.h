#ifndef SONGS_H
#define SONGS_H

#include "globals.h"

#define Song const uint8_t PROGMEM

Song titleSong[] = {
    // total song in bytes = 190
    0x12, // Number of tracks
    0x00,
    0x00, // Address of track 0     //0
    0x03,
    0x00, // Address of track 1     //3
    0x0B,
    0x00, // Address of track 2     //11
    0x22,
    0x00, // Address of track 3     //34
    0x29,
    0x00, // Address of track 4     //41
    0x30,
    0x00, // Address of track 5     //48
    0x37,
    0x00, // Address of track 6     //55
    0x3E,
    0x00, // Address of track 7     //62
    0x50,
    0x00, // Address of track 8     //80
    0x52,
    0x00, // Address of track 9     //82
    0x65,
    0x00, // Address of track 10    //101
    0x68,
    0x00, // Address of track 11    //104
    0x6B,
    0x00, // Address of track 12    //107
    0x6E,
    0x00, // Address of track 13    //110
    0x71,
    0x00, // Address of track 14    //113
    0x78,
    0x00, // Address of track 15    //120
    0x89,
    0x00, // Address of track 16    //137
    0x8F,
    0x00, // Address of track 17    //143

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
    (uint8_t)-2, // FX: VOLUME SLIDE ON: steps = -2
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
    (uint8_t)-1, // FX: SLIDE VOLUME ON: -1
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
    (uint8_t)-8, // FX: VOLUME SLIDE ON: steps = -8
    0x9F + 6, // DELAY: ticks = 6
    0x43, // FX: VOLUME SLIDE OFF
    0x9F + 42, // DELAY: ticks = 42
    0xFE, // RETURN

    //"Track 17"
    0x41,
    (uint8_t)-1, // FX: VOLUME SLIDE ON: steps = -1
    0x9F + 32, // DELAY: ticks = 32
    0x43, // FX: VOLUME SLIDE OFF
    0x9F + 16, // DELAY: ticks = 16
    0xFE, // RETURN
};

Song nameSong[] = {
    // total song in bytes = 49
    0x04, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x03,
    0x00, // Address of track 1
    0x0B,
    0x00, // Address of track 2
    0x1A,
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
    (uint8_t)-8, // slide volume on
    0xFD,
    1,
    3, // REPEAT: count = 1 + 1 / track = 1
    0x4C,
    (uint8_t)-7, // FX: SET TRANSPOSITION: notes = -7
    0xFC,
    3, // GOTO track 3
    0x4C,
    (uint8_t)-5, // FX: SET TRANSPOSITION: notes = -5
    0xFC,
    3, // GOTO track 3
    0x4D, // FX: TRANSPOSITION OFF
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

Song badNews[] = {
    // total song in bytes = 66
    0x07, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x0A,
    0x00, // Address of track 1
    0x12,
    0x00, // Address of track 2
    0x15,
    0x00, // Address of track 3
    0x1E,
    0x00, // Address of track 4
    0x25,
    0x00, // Address of track 5
    0x2A,
    0x00, // Address of track 6

    0x00, // Channel 0 entry track
    0x01, // Channel 1 entry track
    0x02, // Channel 2 entry track
    0x02, // Channel 3 entry track

    //"Track 0"
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    (uint8_t)-1, // FX: VOLUME SLIDE ON: steps = -1
    0x9D,
    24, // SET song tempo: value = 24
    0xFD,
    63,
    3, // REPEAT: count = 63 + 1 / track = 3
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x40,
    24, // FX: SET VOLUME: volume = 24
    0x41,
    (uint8_t)-3, // FX: VOLUME SLIDE ON: steps = -3
    0xFD,
    63,
    4, // REPEAT: count = 63 + 1 / track = 4
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0x40,
    0, // FX: SET VOLUME: volume = 0
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 3"
    0x00 + 1, // NOTE ON: note = 1
    0x9F + 48, // DELAY: ticks = 8
    0x00 + 6, // NOTE ON: note = 6
    0x9F + 48, // DELAY: ticks = 8
    0x00 + 5, // NOTE ON: note = 5
    0x9F + 48, // DELAY: ticks = 8
    0x00 + 10, // NOTE ON: note = 10
    0x9F + 48, // DELAY: ticks = 8
    0xFE, // RETURN

    //"Track 4"
    0xFD,
    2,
    6, // REPEAT: count = 2 + 1 / track = 6
    0xFD,
    2,
    5, // REPEAT: count = 2 + 1 / track = 5
    0xFE, // RETURN

    //"Track 5"                   // ticks = 96 / bytes = 11
    0x00 + 45, // NOTE ON: note = 45
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 42, // NOTE ON: note = 42
    0x9F + 8, // DELAY: ticks = 8
    0xFE, // RETURN

    //"Track 6"
    0x00 + 45, // NOTE ON: note = 45
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 41, // NOTE ON: note = 41
    0x9F + 8, // DELAY: ticks = 8
    0xFE, // RETURN
};

Song fieldSong[] = {
    // total song in bytes = 53
    0x04, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x03,
    0x00, // Address of track 1 //3
    0x12,
    0x00, // Address of track 2 //17
    0x21,
    0x00, // Address of track 2 //31

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
    (uint8_t)-12, // FX: VOLUME SLIDE ON: steps = -12
    0xFD,
    63,
    2, // REPEAT: count = 63 + 1 / track = 2
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0xFD,
    3,
    3, // REPEAT: count = 3 + 1 / track = 3
    0x4C,
    (uint8_t)-7, // FX: SET TRANSPOSITION: notes = -7
    0xFD,
    1,
    3, // REPEAT: count = 1 + 1 / track = 3
    0x4C,
    (uint8_t)-2, // FX: SET TRANSPOSITION: notes = -2
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
    (uint8_t)-6, // FX: VOLUME SLIDE ON: steps = -6
    0xFD,
    63,
    3, // REPEAT: count = 63 + 1 / track = 3
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    (uint8_t)-1, // FX: VOLUME SLIDE ON: steps = -1
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
    (uint8_t)-2, // FX: SET TRANSPOSITION: notes = -2
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

Song darkForest[] = {
    // total song in bytes = 114
    0x09, // Number of tracks
    0x00,
    0x00, // Address of track 0
    0x23,
    0x00, // Address of track 1 //35
    0x29,
    0x00, // Address of track 2 //41
    0x2C,
    0x00, // Address of track 3 //44
    0x30,
    0x00, // Address of track 4 //48
    0x37,
    0x00, // Address of track 5 //55
    0x47,
    0x00, // Address of track 6 //71
    0x4F,
    0x00, // Address of track 7 //79
    0x56,
    0x00, // Address of track 8 //86

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
    (uint8_t)-6, // FX: VOLUME SLIDE ON: steps = -6
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
    (uint8_t)-1, // FX: SET TRANSPOSITION: notes = -1
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
    (uint8_t)-1, // FX: VOLUME SLIDE ON: steps = -1
    0x00 + 2, // NOTE ON: note = 2
    0x9F + 64, // DELAY: ticks = 64
    0xFE, // RETURN

    //"Track 5"
    0xFC,
    6, // GOTO track 6
    0x4C,
    (uint8_t)-4, // FX: SET TRANSPOSITION: notes = -4
    0xFC,
    6, // GOTO track 6
    0x4C,
    (uint8_t)-5, // FX: SET TRANSPOSITION: notes = -5
    0xFC,
    6, // GOTO track 6
    0x4C,
    (uint8_t)-7, // FX: SET TRANSPOSITION: notes = -7
    0xFC,
    6, // GOTO track 6
    0x4D, // FX: TRANSPOSITION OFF
    0xFE, // RETURN

    //"Track 6"
    0x9F + 16, // DELAY: ticks = 16
    0x40,
    32, // FX: SET VOLUME: volume = 32
    0x41,
    (uint8_t)-1, // FX: VOLUME SLIDE ON: steps = -1
    0x00 + 21, // NOTE ON: note = 21
    0x9F + 48, // DELAY: ticks = 48
    0xFE, // RETURN

    //"Track 7"
    0x9F + 48, // DELAY: ticks = 48
    0x41,
    (uint8_t)-8, // FX: VOLUME SLIDE ON: steps = -8
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

Song battleSong[] = {
    // total song in bytes = 112
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

    //"Track 0"
    0x9E,
    0,
    1,
    1,
    1, // FX: GOTO advanced: ch0 = 0x00 / ch1 = 0x01 / ch2 = 0x02 / ch3 = 0x03
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    (uint8_t)-8, // FX: SLIDE VOLUME ON: -8
    0x50,
    5,
    1, // FX: SET VIBRATO: 5 1
    0x9D,
    24, // FX: SET TEMPO: tempo = 24
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    (uint8_t)-2, // FX: SET TRANSPOSITION: -2
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    (uint8_t)-2, // FX: SET TRANSPOSITION: -2
    0xFD,
    1,
    2, // REPEAT: count = 1 + 1 / track = 2
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFC,
    3, // GOTO track 3
    0x4C,
    (uint8_t)-4, // FX: SET TRANSPOSITION: -4
    0xFC,
    3, // GOTO track 3
    0x4C,
    (uint8_t)-2, // FX: SET TRANSPOSITION: -2
    0xFC,
    3, // GOTO track 3
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFD,
    1,
    3, // REPEAT: count = 1 + 1 / track = 3
    0x4C,
    (uint8_t)-4, // FX: SET TRANSPOSITION: -4
    0xFC,
    3, // GOTO track 3
    0x4C,
    (uint8_t)-2, // FX: SET TRANSPOSITION: -2
    0xFC,
    3, // GOTO track 3
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFC,
    3, // GOTO track 3
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
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

    //"Track 3"
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

Song swampSong[] = {
    // total song in bytes = 101

    0x08, // Number of tracks
    0x00,
    0, // Address of track 0
    0x0D,
    0, // Address of track 1 // 13
    0x13,
    0, // Address of track 2 // 19

    0x18,
    0, // Address of track 3 // 24
    0x1B,
    0, // Address of track 4 // 27
    0x30,
    0, // Address of track 5 // 48
    0x3C,
    0, // Address of track 6 // 60
    0x44,
    0, // Address of track 7 // 68

    0x00, // Channel 0 entry track (PULSE)
    0x02, // Channel 1 entry track (SQUARE)
    0x01, // Channel 2 entry track (TRIANGLE)
    0x03, // Channel 3 entry track (NOISE)

    //"Track 0"
    0x9E,
    0,
    2,
    1,
    3, // FX: GOTO advanced: ch0 = 0x01 / ch1 = 0x02 / ch2 = 0x00 / ch3 = 0x03
    0x40,
    32, // FX: SET VOLUME: volume = 48
    0x41,
    (uint8_t)-4, // FX: SLIDE VOLUME ON: -4
    0xFD,
    31,
    7, // REPEAT: count = 31 + 1 / track = 7
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 1"
    0x9D,
    48, // SET song tempo: value = 48
    0xFD,
    7,
    4, // REPEAT: count = 7 + 1 / track = 4   (4 * 512 ticks)
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 2"
    0xFD,
    63,
    6, // REPEAT: count = 63 + 1 / track = 5  (32 * 64 ticks)
    0x00, // NOTE OFF
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 3"                   // ticks = 2048 / bytes = 7
    0x40,
    0, // FX: SET VOLUME: volume = 0
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 4"                   // ticks = 512 / bytes = 21
    0xFD,
    1,
    5, // REPEAT: count = 1 + 1 / track = 5   (2 * 64 ticks)
    0x4B,
    3, // FX: ADD TRANSPOSITION: notes = 3
    0xFD,
    1,
    5, // REPEAT: count = 1 + 1 / track = 5   (2 * 64 ticks)
    0x4B,
    (uint8_t)-1, // FX: ADD TRANSPOSITION: notes = 3
    0xFD,
    1,
    5, // REPEAT: count = 1 + 1 / track = 5   (2 * 64 ticks)
    0x4B,
    3, // FX: ADD TRANSPOSITION: notes = 3
    0xFD,
    1,
    5, // REPEAT: count = 1 + 1 / track = 5   (2 * 64 ticks)
    0x4B,
    (uint8_t)-5, // FX: ADD TRANSPOSITION: notes = 3
    0xFE, // RETURN

    //"Track 5"                   // ticks = 64 / bytes = 14
    0x00 + 49, // NOTE ON: note = 49
    0x40,
    63, // FX: SET VOLUME: volume = 63
    0x41,
    (uint8_t)-16, // FX: VOLUME SLIDE ON: steps = -8
    0x9F + 16, // DELAY: 16 ticks
    0x40,
    16, // FX: SET VOLUME: volume = 16
    0x41,
    (uint8_t)-4, // FX: VOLUME SLIDE ON: steps = -4
    0x9F + 48, // DELAY: 4 ticks
    0xFE, // RETURN

    //"track 6"                   // ticks = 64 / bytes = 10
    0x00 + 1, // NOTE ON: note = 1
    0x40,
    32, // FX: SET VOLUME: volume = 8
    0x4E,
    1,
    0x00 + 0x00 +
        31, // SET TREMOLO OR VIBRATO: depth = 1 / retrig = OFF / TorV = TREMOLO / rate = 31 + 1
    0x9F + 64, // DELAY: 64 ticks
    0xFE, // RETURN

    //"Track 7"
    0x00 + 25, // NOTE ON: note = 25
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 32, // NOTE ON: note = 32
    0x9F + 16, // DELAY: ticks = 16
    0x00 + 28, // NOTE ON: note = 28
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 27, // NOTE ON: note = 27
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 25, // NOTE ON: note = 25
    0x9F + 16, // DELAY: ticks = 16
    0x9F + 64, // DELAY: ticks = 64
    0xFE, // RETURN
};

Song canyonSong[] = {
    // total song in bytes = 107
    0x0A, // Number of tracks
    0x00,
    0x00, // Address of track 0   //
    0x10,
    0x00, // Address of track 1   // 16
    0x18,
    0x00, // Address of track 2   // 24
    0x20,
    0x00, // Address of track 3   // 32
    0x21,
    0x00, // Address of track 4   // 33
    0x25,
    0x00, // Address of track 5   // 37

    0x3C,
    0x00, // Address of track 6   // 60
    0x41,
    0x00, // Address of track 7   // 65
    0x48,
    0x00, // Address of track 8   // 72
    0x4F,
    0x00, // Address of track 9   // 79

    0x00, // Channel 0 entry track
    0x01, // Channel 1 entry track
    0x02, // Channel 2 entry track
    0x03, // Channel 3 entry track

    //"Track channel 0"
    0x9E,
    0,
    1,
    2,
    3, // FX: GOTO advanced: ch0 = 0x00 / ch1 = 0x01 / ch2 = 0x02 / ch3 = 0x03
    0x40,
    24, // FX: SET VOLUME: volume = 32
    0x9D,
    26, // FX: SET TEMPO: tempo = 26
    0x4E,
    6,
    11, // FX: SET VIBRATO: 2 63
    0xFD,
    3,
    5, // REPEAT: count = 3 + 1 / track = 5
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track channel 1"
    0x40,
    24, // FX: SET VOLUME: volume = 48
    0x41,
    (uint8_t)-6, // FX: SLIDE VOLUME ON: -6
    0xFD,
    3,
    5, // REPEAT: count = 3 + 1 / track = 5
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track channel 2"
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    (uint8_t)-12, // FX: SLIDE VOLUME ON: -12
    0xFD,
    6,
    8, // REPEAT: count = 6 + 1 / track = 8
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track channel 3"
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track 4"
    0x00 + 18, // NOTE ON: note = 6
    0x9F + 64, // DELAY: ticks = 64
    0x9F + 64, // DELAY: ticks = 64
    0xFE, // RETURN

    //"Track 5"
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFD,
    3,
    6, // REPEAT: count = 3 + 1 / track = 6
    0x4C,
    3, // FX: SET TRANSPOSITION: 3
    0xFD,
    3,
    6, // REPEAT: count = 3 + 1 / track = 6
    0x4C,
    0, // FX: SET TRANSPOSITION: 0
    0xFD,
    3,
    6, // REPEAT: count = 3 + 1 / track = 6
    0x4C,
    5, // FX: SET TRANSPOSITION: 5
    0xFD,
    2,
    6, // REPEAT: count = 2 + 1 / track = 6
    0xFC,
    7, // GOTO track 7
    0xFE, // RETURN

    //"Track 6"
    0x00 + 6, // NOTE ON: note = 6
    0x9F + 8, // DELAY: ticks = 8
    0x00 + 18, // NOTE ON: note = 18
    0x9F + 8, // DELAY: ticks = 8
    0xFE, // RETURN

    //"Track 7"
    0x00 + 16, // NOTE ON: note = 16
    0x9F + 4, // DELAY: ticks = 8
    0x00 + 15, // NOTE ON: note = 15
    0x9F + 4, // DELAY: ticks = 8
    0x00 + 13, // NOTE ON: note = 13
    0x9F + 8, // DELAY: ticks = 8
    0xFE, // RETURN

    //"Track 8"
    0x9F + 64, // DELAY: ticks = 64
    0xFD,
    2,
    9, // REPEAT: count = 2 + 1 / track = 9
    0x9F + 40, // DELAY: ticks = 40
    0x00 + 0, // NOTE ON: note = 0
    0xFE, // RETURN

    //"Track 8"
    0x00 + 42, // NOTE ON: note = 42
    0x9F + 8, // DELAY: ticks = 8
    0xFE, // RETURN

};

void changeSong(byte region) {
    switch(region) {
    case REGION_FIELDS:
    case REGION_LONG_ROAD:
    case REGION_APPLE_FARM:
    case REGION_FIELDS_CANYONS:
    case REGION_FIELDS_SWAMP:
        if(songPlaying != 1) {
            songPlaying = 1;
            ATM.play(fieldSong);
        }
        break;
    case REGION_SWAMP:
    case REGION_SWAMP_ISLAND_ONE:
    case REGION_SWAMP_ISLAND_TWO:
    case REGION_SWAMP_FOREST:
        if(songPlaying != 2) {
            songPlaying = 2;
            ATM.play(swampSong);
        }
        break;
    case REGION_FOREST:
    case REGION_FOREST_CANYON:
        if(songPlaying != 3) {
            songPlaying = 3;
            ATM.play(darkForest);
        }
        break;
    case REGION_CANYONS:
        if(songPlaying != 4) {
            songPlaying = 4;
            ATM.play(canyonSong);
        }
        break;
    case REGION_CAVE_INTERIOR:
        if(songPlaying != 5) {
            songPlaying = 5;
            ATM.play(badNews);
        }
        break;
    case REGION_YOUR_GARDEN:
        if(songPlaying != 6) {
            songPlaying = 6;
            ATM.play(nameSong);
        }
        break;
    default:
        break;
    }
}

#endif
