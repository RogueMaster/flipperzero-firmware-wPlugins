#pragma once
/*
 * fsd_profile.h — user-authored CAN test profiles (.cantest).
 *
 * The goal is the simplest possible authoring loop: a plain text file the user
 * edits on any computer, one frame per line, in the SAME candump format that
 * CAN Capture writes — so a line copied straight out of a capture log can be
 * tweaked and replayed. The Flipper loads it from a menu, runs it (only when
 * the car is provably stationary), and logs the result for a bug report.
 *
 * Line format (whitespace-separated, # starts a comment):
 *
 *     # Name: poke 0x229 park button
 *     229#00112233445566AA  repeat=20  delay=100
 *     3FD#1000000000004000
 *     (1.234) can0 370#0000000000000000      <- a raw candump line also works
 *
 *   ID#DATA      hex CAN id, '#', hex data bytes (0..8 bytes). A leading
 *                "(timestamp) bus " candump prefix is ignored if present.
 *   repeat=N     send the frame N times (default 1)
 *   delay=N      milliseconds between sends (default 50; "delay=100ms" also ok)
 *   # Name: ...   sets the profile's display name
 */

#include "fsd_state.h" // FSDState, CANFRAME / MAX_LEN (via fsd_types.h)
#include <stdbool.h>
#include <stdint.h>

// One transmit step parsed from a profile line.
typedef struct {
    uint32_t can_id;
    uint8_t data[MAX_LEN];
    uint8_t dlc;
    uint16_t repeat; // times to send (>= 1)
    uint16_t delay_ms; // delay between sends
} FsdProfileStep;

typedef enum {
    FSD_PLINE_EMPTY = 0, // blank line or plain comment — skip
    FSD_PLINE_NAME, // "# Name: ..." — name_out filled
    FSD_PLINE_STEP, // a frame to send — *step filled
    FSD_PLINE_ERROR, // malformed line
} FsdProfileLineKind;

// Parse a single profile line. See the format above. name_out (may be NULL) is
// filled only for FSD_PLINE_NAME.
FsdProfileLineKind
    fsd_profile_parse_line(const char* line, FsdProfileStep* step, char* name_out, int name_cap);

// Safety interlock: may a user test profile transmit *right now*?
// Requires: not in Listen-Only, a DI_speed (0x257) frame has been seen, that
// frame is fresh (< ~1 s old), and the car is stationary (speed ~ 0).
// Fail-closed: if no speed data has been seen, transmit is refused — so a
// profile can never inject while the car is moving (or on a bus that doesn't
// carry speed). now_ms is a millisecond clock (Flipper: furi_get_tick, 1 kHz).
bool fsd_profile_tx_allowed(const FSDState* state, uint32_t now_ms);

// Freshness window (ms) for the DI_speed frame used by the interlock.
#define FSD_PROFILE_SPEED_FRESH_MS 1000u
