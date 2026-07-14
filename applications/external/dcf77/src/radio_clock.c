#include <stddef.h>

#include "radio_clock.h"

static const RadioClockSignalInfo radio_clock_signal_info[RadioClockSignalCount] = {
    [RadioClockSignalTest] =
        {
            .label = "Test",
            .default_freq = 77500U,
            .confidence = RadioClockProtocolConfidenceExperimental,
            .tx_supported = true,
        },
    [RadioClockSignalDcf77] =
        {
            .label = "DCF77",
            .default_freq = 77500U,
            .confidence = RadioClockProtocolConfidenceStable,
            .tx_supported = true,
        },
    [RadioClockSignalWwvb] =
        {
            .label = "WWVB",
            .default_freq = 60000U,
            .confidence = RadioClockProtocolConfidenceStable,
            .tx_supported = true,
        },
    [RadioClockSignalBpc] =
        {
            .label = "BPC",
            .default_freq = 68500U,
            .confidence = RadioClockProtocolConfidenceExperimental,
            .tx_supported = true,
        },
    [RadioClockSignalJjy] =
        {
            .label = "JJY",
            .default_freq = 60000U,
            .confidence = RadioClockProtocolConfidenceLikely,
            .tx_supported = true,
        },
    [RadioClockSignalMsf] =
        {
            .label = "MSF",
            .default_freq = 60000U,
            .confidence = RadioClockProtocolConfidenceStable,
            .tx_supported = true,
        },
    [RadioClockSignalRbu] =
        {
            .label = "RBU",
            .default_freq = 67500U,
            .confidence = RadioClockProtocolConfidenceUnsupported,
            .tx_supported = false,
        },
    [RadioClockSignalHbg] =
        {
            .label = "HBG",
            .default_freq = 75000U,
            .confidence = RadioClockProtocolConfidenceLikely,
            .tx_supported = true,
        },
    [RadioClockSignalBsf] =
        {
            .label = "BSF",
            .default_freq = 77500U,
            .confidence = RadioClockProtocolConfidenceExperimental,
            .tx_supported = true,
        },
};

static const RadioClockSignal radio_clock_visible_signals[] = {
    RadioClockSignalTest,
    RadioClockSignalDcf77,
    RadioClockSignalWwvb,
    RadioClockSignalMsf,
    RadioClockSignalJjy,
    RadioClockSignalBpc,
    RadioClockSignalBsf,
    /* Keep HBG last in the selector. */
    RadioClockSignalHbg,
};

const RadioClockSignalInfo* radio_clock_signal_get_info(RadioClockSignal signal) {
    if(signal >= RadioClockSignalCount) {
        signal = RadioClockSignalDcf77;
    }

    return &radio_clock_signal_info[signal];
}

const char* radio_clock_signal_get_label(RadioClockSignal signal) {
    return radio_clock_signal_get_info(signal)->label;
}

uint32_t radio_clock_signal_get_default_freq(RadioClockSignal signal) {
    return radio_clock_signal_get_info(signal)->default_freq;
}

RadioClockProtocolConfidence radio_clock_signal_get_confidence(RadioClockSignal signal) {
    return radio_clock_signal_get_info(signal)->confidence;
}

bool radio_clock_signal_can_run(RadioClockSignal signal) {
    return radio_clock_signal_get_info(signal)->tx_supported;
}

size_t radio_clock_visible_signal_count(void) {
    return sizeof(radio_clock_visible_signals) / sizeof(radio_clock_visible_signals[0]);
}

RadioClockSignal radio_clock_visible_signal_get(size_t index) {
    if(index >= radio_clock_visible_signal_count()) {
        index = 1;
    }

    return radio_clock_visible_signals[index];
}

size_t radio_clock_visible_signal_index(RadioClockSignal signal) {
    const size_t count =
        sizeof(radio_clock_visible_signals) / sizeof(radio_clock_visible_signals[0]);

    for(size_t i = 0; i < count; i++) {
        if(radio_clock_visible_signals[i] == signal) {
            return i;
        }
    }

    return 1;
}
