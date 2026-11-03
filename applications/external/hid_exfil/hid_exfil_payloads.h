#pragma once

#include "hid_exfil.h"

/**
 * Get the injection script for a given payload type and target OS.
 * Returns a heap-allocated string containing the script to be typed.
 * Caller must free() the returned buffer when done.
 * The script collects data on the target and encodes it via keyboard LED toggles.
 *
 * LED encoding protocol:
 *   Each byte is sent as 4 dibits (2-bit pairs), MSB first.
 *   For each dibit:
 *     - Target sets Caps Lock = bit1, Num Lock = bit0
 *     - Target toggles Scroll Lock to clock the dibit
 *   End-of-transmission: target toggles all 3 LEDs simultaneously 3 times.
 */
char* hid_exfil_get_payload_script(PayloadType type, TargetOS os);

/**
 * Get a human-readable label for the payload type.
 */
const char* hid_exfil_get_payload_label(PayloadType type);
