#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Open a LIRC device in SCANCODE mode. Returns fd or -1 on error. */
int wfr_lirc_open(const char* device);

/* Close a LIRC device. */
void wfr_lirc_close(int fd);

/* Read one decoded RC-6 or NEC scancode from the LIRC device.
 * Blocks until a supported scancode is available.
 *
 * address: the 8-bit address field
 * command: the 8-bit command field
 *
 * Returns 0 on success, -1 on error. */
int wfr_lirc_read_scancode(int fd, uint8_t* address, uint8_t* command);
