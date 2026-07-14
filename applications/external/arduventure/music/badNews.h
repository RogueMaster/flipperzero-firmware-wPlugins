#ifndef BADNEWS_H
#define BADNEWS_H

#define Song const uint8_t PROGMEM

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
    -1, // FX: VOLUME SLIDE ON: steps = -1
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
    -3, // FX: VOLUME SLIDE ON: steps = -3
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

#endif
