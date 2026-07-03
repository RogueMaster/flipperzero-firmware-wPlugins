#pragma once

#include "../flipper/protocol/wfr_protocol.h"
#include <stdbool.h>
#include <stdint.h>

/*
 * ACK transmitter — sends status back to Flipper via IR.
 *
 * After connecting (or failing to connect), the daemon transmits an ACK
 * using the same RC-6 protocol framing (START/DATA/END). The Flipper
 * listens for this response and displays the result.
 *
 * Success payload: "OK:192.168.1.102"
 * Failure payload: "FAIL"
 */

/* Open LIRC device for TX (scancode mode). Returns fd or -1 on error. */
int wfr_ack_open(const char* device);

/* Close LIRC TX device. */
void wfr_ack_close(int fd);

/* Send an ACK response via IR.
 * success: true = connected, false = failed
 * ip_str:  IP address string (only used when success=true, can be NULL)
 */
bool wfr_ack_send(int fd, bool success, const char* ip_str);
