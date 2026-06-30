#pragma once

typedef enum {
    // Reserve first 100 events for button types and indexes, starting from 0
    iButtonConverterCustomEventReserved = 100,

    iButtonConverterCustomEventBack,
    iButtonConverterCustomEventTextEditResult,
} iButtonConverterCustomEvent;
