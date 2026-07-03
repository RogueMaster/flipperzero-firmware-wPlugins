#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool eapol_response; /* received any EAPOL response */
    bool eap_request; /* received EAP-Request (auth required) */
    bool eap_success; /* received EAP-Success */
    bool eap_failure; /* received EAP-Failure */
    uint8_t eap_type; /* EAP type from request (1=Identity, etc.) */
    uint8_t auth_mac[6]; /* authenticator MAC address */
    uint16_t frames_seen; /* total EAPOL frames seen */
    bool valid;
} EapolProbeResult;

/**
 * Send EAPOL-Start and listen for 802.1X authenticator response.
 * @param our_mac  Our MAC address
 * @param result   Output result
 * @return true if any EAPOL response received
 */
bool eapol_probe_test(const uint8_t our_mac[6], EapolProbeResult* result);
