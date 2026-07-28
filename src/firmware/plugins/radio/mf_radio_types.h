#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MF_RADIO_DEFAULT_FREQUENCY_HZ 433160000U
#define MF_RADIO_DEFAULT_FREQUENCY_KHZ 433160U

typedef enum {
    MfRadioPageIdle = 0,
    MfRadioPageTransmit,
    MfRadioPageReceive,
    MfRadioPageFrequency,
} MfRadioPage;

typedef enum {
    MfRadioTxIntervalNone = 0,
    MfRadioTxIntervalMark,
    MfRadioTxIntervalSpace,
} MfRadioTxInterval;

