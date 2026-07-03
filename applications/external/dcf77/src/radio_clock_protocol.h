#ifndef RADIO_CLOCK_PROTOCOL_H
#define RADIO_CLOCK_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "radio_clock.h"
#include "radio_clock_pulse.h"

#define RADIO_CLOCK_FRAME_BYTES         8U
#define RADIO_CLOCK_FRAME_SECONDS       60U
#define RADIO_CLOCK_MAX_SECOND_SEGMENTS 8U

typedef struct {
    uint16_t year;
    uint16_t day_of_year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;
    uint8_t hour;
    uint8_t minute;
} RadioClockProtocolTime;

typedef struct __attribute__((packed)) {
    bool level;
    uint16_t ticks;
} RadioClockWaveformSegment;

typedef struct __attribute__((packed)) {
    uint8_t segment_count;
    RadioClockWaveformSegment segments[RADIO_CLOCK_MAX_SECOND_SEGMENTS];
} RadioClockSecondWaveform;

typedef struct {
    uint8_t encoded[RADIO_CLOCK_FRAME_BYTES];
    RadioClockPulse pulses[RADIO_CLOCK_FRAME_SECONDS];
    RadioClockSecondWaveform waveforms[RADIO_CLOCK_FRAME_SECONDS];
} RadioClockMinuteFrame;

typedef struct {
    void (*prepare_frame)(RadioClockMinuteFrame* frame, const RadioClockProtocolTime* time);
} RadioClockProtocolOps;

const RadioClockProtocolOps* radio_clock_protocol_get(RadioClockSignal signal);
bool radio_clock_protocol_uses_following_minute(RadioClockSignal signal);
const char* radio_clock_protocol_pulse_label(RadioClockPulse pulse);
uint16_t radio_clock_protocol_day_of_year(uint16_t year, uint8_t month, uint8_t day);
void radio_clock_protocol_prepare_frame(
    RadioClockSignal signal,
    RadioClockMinuteFrame* frame,
    const RadioClockProtocolTime* time);

#endif
