#pragma once

#include "protocol/wfr_protocol.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transmit WiFi credentials over IR.
 *
 * Builds the WiFi QR string, sends it as a sequence of IR messages
 * (START + DATA bytes + END), repeated WFR_RETRANSMIT_COUNT times.
 * Uses RC-6 or NEC encoding based on the protocol parameter.
 *
 * Returns true if transmission completed, false on error.
 */
bool wfr_transmit_credentials(const WfrWifiCreds* creds, WfrIrProtocol protocol);

#ifdef __cplusplus
}
#endif
