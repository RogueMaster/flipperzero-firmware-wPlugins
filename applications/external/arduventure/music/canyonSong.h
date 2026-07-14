
#ifndef SONG_H
#define SONG_H

#define Song const uint8_t PROGMEM

Song music[] = {
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
    -6, // FX: SLIDE VOLUME ON: -6
    0xFD,
    3,
    5, // REPEAT: count = 3 + 1 / track = 5
    0x9F, // FX: STOP CURRENT CHANNEL

    //"Track channel 2"
    0x40,
    48, // FX: SET VOLUME: volume = 48
    0x41,
    -12, // FX: SLIDE VOLUME ON: -12
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
    4,
    9, // REPEAT: count = 2 + 1 / track = 9
    0x9F + 24, // DELAY: ticks = 40
    0x00 + 0, // NOTE ON: note = 0
    0xFE, // RETURN

    //"Track 8"
    0x00 + 42, // NOTE ON: note = 42
    0x9F + 8, // DELAY: ticks = 8
    0xFE, // RETURN

};
