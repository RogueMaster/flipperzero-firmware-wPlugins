#pragma once

// All modem timings/tolerances for the 125 kHz LF Manchester-ASK line code.
// Pure numeric constants, no furi includes, so rfid_modem.c stays host-testable.

// Carrier: 125 kHz -> one carrier cycle = 8 us. The emulate DMA expresses runs
// in carrier cycles; the capture path measures them in microseconds.
#define RFID_MODEM_CARRIER_US 8u

// Line code: Manchester at RF/32. Bit period = 32 carrier cycles = 256 us; each
// half-bit is 16 cycles = 128 us. (RF/16 is a v2 upgrade after bench validation.)
#define RFID_MODEM_RF_K 32u
#define RFID_MODEM_HALFBIT_CYCLES (RFID_MODEM_RF_K / 2u) // 16 cycles
#define RFID_MODEM_HALFBIT_US (RFID_MODEM_HALFBIT_CYCLES * RFID_MODEM_CARRIER_US) // 128 us
#define RFID_MODEM_FULLBIT_US (RFID_MODEM_RF_K * RFID_MODEM_CARRIER_US)           // 256 us

// Run-length classification tolerance (percent) around the half-bit / full-bit
// nominal durations. Weak coupling widens the jitter, so keep this generous.
#define RFID_MODEM_TOLERANCE_PCT 25u

// Any level run longer than this is treated as an inter-frame gap and resets the
// decoder to sync-hunt (~2.5 bit periods).
#define RFID_MODEM_GAP_US ((RFID_MODEM_FULLBIT_US * 5u) / 2u) // 640 us

// Frame: [preamble: PREAMBLE_BITS ones][sync 0x7E][len][packet bytes], LSB-first.
#define RFID_MODEM_PREAMBLE_BITS 16u
// The decoder locks on the sync byte preceded by this many preamble ones, so it
// tolerates losing up to (PREAMBLE_BITS - PREAMBLE_MATCH_BITS) early edges while
// still rejecting random data as a false preamble.
#define RFID_MODEM_PREAMBLE_MATCH_BITS 8u
#define RFID_MODEM_SYNC_BYTE 0x7Eu

// Inter-frame gap emitted by the encoder after each frame (bit periods of load off).
#define RFID_MODEM_IFG_BITS 4u
