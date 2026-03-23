#pragma once

#include "protocol/wfr_protocol.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Transmit WiFi credentials over IR using RC-6 protocol messages.
 *
 * Builds the WiFi QR string, sends it as a sequence of RC-6 messages
 * (START + DATA bytes + END), repeated WFR_RETRANSMIT_COUNT times.
 *
 * Returns true if transmission completed, false on error.
 */
bool wfr_transmit_credentials(const WfrWifiCreds* creds);

#ifdef __cplusplus
}
#endif
