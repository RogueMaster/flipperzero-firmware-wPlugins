#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Open an evdev input device for reading IR scancodes. Returns fd or -1 on error. */
int wfr_evdev_open(const char* device);

/* Close an evdev device. */
void wfr_evdev_close(int fd);

/* Read one NEC IR scancode from an evdev device (MSC_RAW events).
 * Blocks until a valid NEC scancode is available.
 * Performs FAB4-style bit-reversal and NEC inverse validation.
 *
 * address: the decoded 8-bit address field
 * command: the decoded 8-bit command field
 *
 * Returns 0 on success, -1 on error. */
int wfr_evdev_read_scancode(int fd, uint8_t* address, uint8_t* command);
