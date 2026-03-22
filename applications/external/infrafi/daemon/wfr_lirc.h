#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Open a LIRC device in SCANCODE mode. Returns fd or -1 on error. */
int wfr_lirc_open(const char* device);

/* Close a LIRC device. */
void wfr_lirc_close(int fd);

/* Read one decoded RC-6 scancode from the LIRC device.
 * Blocks until an RC-6 scancode is available.
 *
 * rc6_address: the 8-bit address field
 * rc6_command: the 8-bit command field
 *
 * Returns 0 on success, -1 on error. */
int wfr_lirc_read_scancode(int fd, uint8_t* rc6_address, uint8_t* rc6_command);
