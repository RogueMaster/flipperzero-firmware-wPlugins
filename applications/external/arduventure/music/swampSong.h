#ifndef SONG_H
#define SONG_H

#define Song const uint8_t PROGMEM

Song music[] = {
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
    -4, // FX: SLIDE VOLUME ON: -4
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
    -1, // FX: ADD TRANSPOSITION: notes = 3
    0xFD,
    1,
    5, // REPEAT: count = 1 + 1 / track = 5   (2 * 64 ticks)
    0x4B,
    3, // FX: ADD TRANSPOSITION: notes = 3
    0xFD,
    1,
    5, // REPEAT: count = 1 + 1 / track = 5   (2 * 64 ticks)
    0x4B,
    -5, // FX: ADD TRANSPOSITION: notes = 3
    0xFE, // RETURN

    //"Track 5"                   // ticks = 64 / bytes = 14
    0x00 + 49, // NOTE ON: note = 49
    0x40,
    63, // FX: SET VOLUME: volume = 63
    0x41,
    -16, // FX: VOLUME SLIDE ON: steps = -8
    0x9F + 16, // DELAY: 16 ticks
    0x40,
    16, // FX: SET VOLUME: volume = 16
    0x41,
    -4, // FX: VOLUME SLIDE ON: steps = -4
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

#endif
