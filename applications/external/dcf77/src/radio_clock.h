#ifndef RADIO_CLOCK_H
#define RADIO_CLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RadioClockSignalTest,
    RadioClockSignalDcf77,
    RadioClockSignalWwvb,
    RadioClockSignalBpc,
    RadioClockSignalJjy,
    RadioClockSignalMsf,
    RadioClockSignalRbu,
    RadioClockSignalHbg,
    RadioClockSignalBsf,
    RadioClockSignalCount,
} RadioClockSignal;

typedef enum {
    RadioClockProtocolConfidenceExperimental,
    RadioClockProtocolConfidenceLikely,
    RadioClockProtocolConfidenceStable,
    RadioClockProtocolConfidenceUnsupported,
} RadioClockProtocolConfidence;

typedef struct {
    const char* label;
    uint32_t default_freq;
    RadioClockProtocolConfidence confidence;
    bool tx_supported;
} RadioClockSignalInfo;

const RadioClockSignalInfo* radio_clock_signal_get_info(RadioClockSignal signal);
const char* radio_clock_signal_get_label(RadioClockSignal signal);
uint32_t radio_clock_signal_get_default_freq(RadioClockSignal signal);
RadioClockProtocolConfidence radio_clock_signal_get_confidence(RadioClockSignal signal);
bool radio_clock_signal_can_run(RadioClockSignal signal);
size_t radio_clock_visible_signal_count(void);
RadioClockSignal radio_clock_visible_signal_get(size_t index);
size_t radio_clock_visible_signal_index(RadioClockSignal signal);

#endif
