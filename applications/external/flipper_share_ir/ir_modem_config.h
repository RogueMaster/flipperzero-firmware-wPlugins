#pragma once

// IR modem tunables. All durations are in microseconds unless stated otherwise.
// This is the single place to tweak the physical layer; changing anything here
// requires a recompile (there are intentionally no GUI settings).
//
// Line code: a fixed short "mark" (carrier burst) acts as a clock tick, and the
// following "space" (gap) carries the information as one of N discrete levels
// (pulse-position / pulse-distance modulation). Information is put into spaces
// because a demodulating TSOP receiver reports gap lengths more cleanly than
// burst lengths (AGC and turn-on lag distort marks more).
//
// Collision avoidance: the data mark (~400 us) sits between the data-bit marks
// of consumer IR protocols, and the SYNC mark (~1800 us) matches no protocol
// preamble (nearest is Sony SIRC at 2400 us, clear by > the decoder tolerance),
// so nearby TVs/AV gear should not decode our frames as valid commands.

// --- Carrier (TX only; the TSOP demodulates a fixed ~38 kHz band on RX) ---
#define IR_MODEM_CARRIER_HZ 38000u
#define IR_MODEM_DUTY_CYCLE 0.33f

// --- Symbol encoding: fixed mark + (1<<BITS) level space ---
#define IR_MODEM_BITS_PER_SYMBOL 3u // 2..4 supported; levels = 1 << BITS
#define IR_MODEM_MARK_US         400u // fixed data mark (clock tick)
#define IR_MODEM_SPACE_BASE_US   400u // space for level 0
#define IR_MODEM_SPACE_STEP_US   350u // step between adjacent space levels

// --- Framing ---
#define IR_MODEM_SYNC_MARK_US  1800u // frame-start mark (unlike any IR protocol preamble)
#define IR_MODEM_SYNC_SPACE_US 900u // space after the sync mark
#define IR_MODEM_END_MARK_US   400u // trailing mark that closes the final data space
#define IR_MODEM_LEAD_GAP_US   10000u // leading silence; guarantees inter-frame separation

// --- RX framing/timeout ---
// Silence for this long delimits a frame. Must be strictly greater than the
// longest intra-frame space (so a max-level space does not end a frame early)
// and strictly less than IR_MODEM_LEAD_GAP_US (so consecutive frames split).
#define IR_MODEM_RX_TIMEOUT_US    6000u
#define IR_MODEM_SYNC_MARK_TOL_US 600u // acceptance window around the sync mark on RX

// --- Data whitening ---
// XOR every packet byte with a fixed LFSR keystream so that even highly
// repetitive payloads produce an irregular modulated envelope. This dodges the
// TSOP AGC's suppression of regular ("noise-like") signals. Reversible: the same
// routine encodes and decodes. Set to 0 to send raw bytes (useful for debugging).
#define IR_MODEM_WHITENING 1

// --- Sizing limits (used to bound static/heap buffers) ---
// Largest packet the modem will ever carry (control packet with a 36-char name).
#define IR_MODEM_MAX_FRAME_BYTES 96u

// Derived
#define IR_MODEM_LEVELS      (1u << IR_MODEM_BITS_PER_SYMBOL)
// Worst case (BITS==2): 2 timings per symbol, 1 symbol per 2 bits -> 8 timings/byte,
// plus lead + 2 sync + end.
#define IR_MODEM_MAX_TIMINGS ((IR_MODEM_MAX_FRAME_BYTES * 8u) + 8u)
// A frame's captured events cannot exceed the number of timings by much; add slack
// for leading garbage before the sync mark.
#define IR_MODEM_MAX_EVENTS  (IR_MODEM_MAX_TIMINGS + 64u)
