#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DHCP_FP_MAX_CLIENTS 32
#define DHCP_FP_MAX_OPTIONS 16

typedef struct {
    uint8_t mac[6];
    char os_guess[32]; /* identified OS or device type */
    bool identified;
} DhcpFpClient;

typedef struct {
    DhcpFpClient clients[DHCP_FP_MAX_CLIENTS];
    uint16_t client_count;
    uint16_t total_discovers;
} DhcpFpState;

/**
 * Initialize DHCP fingerprint state.
 */
void dhcp_fp_init(DhcpFpState* state);

/**
 * Process a raw Ethernet frame for DHCP Discover option 55 fingerprinting.
 * @param state  Fingerprint state
 * @param frame  Raw Ethernet frame
 * @param len    Frame length
 * @return true if a new client was fingerprinted
 */
bool dhcp_fp_process_frame(DhcpFpState* state, const uint8_t* frame, uint16_t len);
