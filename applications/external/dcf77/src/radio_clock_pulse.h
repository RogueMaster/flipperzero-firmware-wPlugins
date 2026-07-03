#ifndef RADIO_CLOCK_PULSE_H
#define RADIO_CLOCK_PULSE_H

typedef enum {
    RadioClockPulseZero,
    RadioClockPulseOne,
    RadioClockPulseMarker,
    RadioClockPulseMsfA0B0,
    RadioClockPulseMsfA0B1,
    RadioClockPulseMsfA1B0,
    RadioClockPulseMsfA1B1,
    RadioClockPulsePair00,
    RadioClockPulsePair01,
    RadioClockPulsePair10,
    RadioClockPulsePair11,
} RadioClockPulse;

#endif
