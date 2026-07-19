#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Live 1-Wire (Dallas / iButton) front-end. Presses of a genuine key on the
 * Flipper's iButton pads are read with the standard READ ROM (0x33) command:
 * reset -> presence -> 8-byte ROM. The 8th byte is a Maxim CRC-8 over the first
 * seven; we recompute it on-device so the walkthrough's "family / serial / CRC"
 * story is proven live. Read-only: nothing is written to the key. */

typedef enum {
    OneWireIdle,
    OneWireScanning,
    OneWireReady,
} OneWireReaderState;

typedef struct {
    uint8_t rom[8]; // family(1) + serial(6) + crc(1), LSB-first as read
    uint8_t crc_calc; // CRC-8 we computed over rom[0..6]
    bool crc_ok; // crc_calc == rom[7]
} OneWireReading;

typedef struct OneWireReader OneWireReader;

OneWireReader* onewire_reader_alloc(void);
void onewire_reader_free(OneWireReader* r);

void onewire_reader_start(OneWireReader* r);
void onewire_reader_stop(OneWireReader* r);

OneWireReaderState onewire_reader_state(OneWireReader* r);
bool onewire_reader_get(OneWireReader* r, OneWireReading* out);
