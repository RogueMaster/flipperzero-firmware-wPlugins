#pragma once

#include "tpms_protocol.h"

/* What the radio can be set to. Both the app and the screen need this,
 * and so does the layout test, which builds the screen without the rest
 * of the app around it. */

/** Bands these sensors use: 433.92 MHz in Europe, 315 MHz in North
 * America and much of Asia. The radio listens on one at a time. */
#define TPMS_FREQUENCY_COUNT 2
extern const uint32_t tpms_frequencies[TPMS_FREQUENCY_COUNT];

/** What the radio is set up for. The first four are the combinations of
 * band and modulation; the last one steps through all four in turn, which
 * finds a sensor whose kind is not known in advance at the cost of
 * hearing each of them a quarter of the time. */
typedef enum {
    TpmsConfig433Fsk,
    TpmsConfig433Ook,
    TpmsConfig315Fsk,
    TpmsConfig315Ook,
    TpmsConfigScan,
    TpmsConfigCount,
} TpmsConfig;

/** The fixed combinations, which are also the steps of the scan. */
#define TPMS_SLOT_COUNT 4

/** How long each combination gets while scanning, ms. */
#define TPMS_SCAN_PERIOD_MS 4000

/** Band and modulation of one of the four combinations. */
uint32_t tpms_slot_frequency(uint8_t slot);
TpmsModulation tpms_slot_modulation(uint8_t slot);
