#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_types.h>

/* The standard rates Hermes snaps a raw measurement onto. Ordered ascending;
 * the fit walks the whole table, so extra entries only cost a comparison. */
#define HERMES_BAUD_COUNT (17u)

typedef struct {
    uint32_t baud;
    const char* note; // shown under the result, "" when the rate needs no gloss
} HermesBaudEntry;

extern const HermesBaudEntry hermes_baud_table[HERMES_BAUD_COUNT];

/** Index of the rate closest to `baud`, by relative error. */
size_t hermes_baud_nearest_index(uint32_t baud);

/* ------------------------------------------------------------- framing ---- */

typedef enum {
    HermesFraming8N1,
    HermesFraming8E1,
    HermesFraming8O1,
    HermesFraming7E1,

    HermesFramingCount,
} HermesFraming;

const char* hermes_framing_name(HermesFraming framing);

/** Push a framing onto an initialised serial handle. */
void hermes_framing_apply(FuriHalSerialHandle* handle, HermesFraming framing);

/* ---------------------------------------------------------------- port ---- */

typedef enum {
    HermesPortUsart, // pin 13 TX / pin 14 RX  - the fast one, up to 921600
    HermesPortLpuart, // pin 15 TX / pin 16 RX  - low-power, kinder to slow rates

    HermesPortCount,
} HermesPort;

const char* hermes_port_name(HermesPort port);

/** Human wiring hint, e.g. "RX=14 TX=13". */
const char* hermes_port_pins(HermesPort port);

FuriHalSerialId hermes_port_serial_id(HermesPort port);

/** The RX pin, borrowed as a plain interrupt input during baud detection. */
const GpioPin* hermes_port_rx_pin(HermesPort port);
