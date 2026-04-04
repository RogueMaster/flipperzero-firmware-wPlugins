#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ROGUE_DHCP_MAX_SERVERS 8

typedef struct {
    uint8_t server_ip[4];
    uint8_t server_mac[6];
    uint8_t offered_ip[4];
    uint8_t gateway[4];
    uint8_t dns[4];
    char domain[32];
    uint32_t lease_time;
    uint16_t offer_count; /* how many Offers from this server */
} RogueDhcpServer;

typedef struct {
    RogueDhcpServer servers[ROGUE_DHCP_MAX_SERVERS];
    uint8_t server_count;
    uint16_t discover_sent;
    uint16_t offers_received;
    bool multiple_servers; /* true if more than one DHCP server responded */
} RogueDhcpState;

/**
 * Send DHCP Discover and collect Offers from multiple servers.
 * Detects rogue DHCP servers by receiving multiple Offer responses.
 * @param our_mac     Our MAC address
 * @param state       Output state with detected servers
 * @param listen_ms   How long to listen for Offers (recommended: 5000)
 * @return true if any Offers received
 */
bool rogue_dhcp_detect(const uint8_t our_mac[6], RogueDhcpState* state, uint32_t listen_ms);
