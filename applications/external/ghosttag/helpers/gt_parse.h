#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ble_signatures.h"

/**
 * The ESP32 wire-protocol parser, kept free of any Flipper dependency.
 *
 * This is the one piece of GhostTag that reads attacker-reachable input: the
 * bytes on the UART come from whatever is wired to the GPIO, which is not
 * necessarily the firmware in this repository. Splitting it out of
 * uart_link.c means it can be compiled and fuzzed on a laptop - see
 * test/test_parse.c - rather than only ever being exercised on a device where
 * a read past the end of a buffer shows up as a reboot.
 */

typedef struct {
    uint8_t mac[6];
    TrackerType type;
    int8_t rssi;
    char name[20];
} GtDetection;

/**
 * Parse one "GT1,<mac12hex>,<rssi>,<type>[,<name>]" line.
 *
 * @param line  NUL-terminated, WITHOUT the "GT1," prefix. Not modified.
 * @return false if any field is missing or malformed. @p out is untouched
 *         unless the whole line parsed.
 */
bool gt_parse_detection(const char* line, GtDetection* out);

/** True for a line that is a "GT1," detection. */
bool gt_line_is_detection(const char* line);

/** True for "GTHELLO,<version>"; @p version_out gets the rest of the line. */
bool gt_line_is_hello(const char* line, const char** version_out);

/** True for a "GTALIVE" heartbeat. */
bool gt_line_is_alive(const char* line);
