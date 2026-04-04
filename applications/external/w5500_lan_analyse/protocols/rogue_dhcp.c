#include "rogue_dhcp.h"
#include "../utils/packet_utils.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <socket.h>
#include <string.h>

#define DHCP_SOCK         1
#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68
#define DHCP_MAGIC_COOKIE 0x63825363

/* DHCP message types */
#define DHCP_DISCOVER 1
#define DHCP_OFFER    2

/**
 * Build a DHCP Discover packet for rogue detection.
 */
static uint16_t
    build_discover(uint8_t* pkt, uint16_t pkt_size, const uint8_t mac[6], uint32_t xid) {
    if(pkt_size < 300) return 0;
    memset(pkt, 0, 300);

    pkt[0] = 1; /* op: BOOTREQUEST */
    pkt[1] = 1; /* htype: Ethernet */
    pkt[2] = 6; /* hlen */
    pkt[3] = 0; /* hops */

    pkt_write_u32_be(&pkt[4], xid);
    pkt_write_u16_be(&pkt[8], 0); /* secs */
    pkt_write_u16_be(&pkt[10], 0x8000); /* flags: broadcast */

    memcpy(&pkt[28], mac, 6); /* chaddr */

    /* Magic cookie at offset 236 */
    pkt_write_u32_be(&pkt[236], DHCP_MAGIC_COOKIE);

    /* DHCP options starting at offset 240 */
    uint16_t idx = 240;

    /* Option 53: DHCP Message Type = Discover */
    pkt[idx++] = 53;
    pkt[idx++] = 1;
    pkt[idx++] = DHCP_DISCOVER;

    /* Option 55: Parameter Request List */
    pkt[idx++] = 55;
    pkt[idx++] = 6;
    pkt[idx++] = 1; /* Subnet Mask */
    pkt[idx++] = 3; /* Router */
    pkt[idx++] = 6; /* DNS */
    pkt[idx++] = 15; /* Domain Name */
    pkt[idx++] = 51; /* Lease Time */
    pkt[idx++] = 54; /* Server Identifier */

    /* End option */
    pkt[idx++] = 255;

    return idx;
}

/**
 * Parse a DHCP Offer and extract server information.
 */
static bool parse_offer(const uint8_t* pkt, uint16_t len, uint32_t xid, RogueDhcpServer* server) {
    if(len < 244) return false;

    /* Check op = BOOTREPLY */
    if(pkt[0] != 2) return false;

    /* Check XID */
    if(pkt_read_u32_be(&pkt[4]) != xid) return false;

    /* Check magic cookie */
    if(pkt_read_u32_be(&pkt[236]) != DHCP_MAGIC_COOKIE) return false;

    /* Offered IP (yiaddr) */
    memcpy(server->offered_ip, &pkt[16], 4);

    /* Server IP (siaddr) */
    memcpy(server->server_ip, &pkt[20], 4);

    /* Parse options */
    uint16_t idx = 240;
    bool is_offer = false;

    while(idx < len && pkt[idx] != 255) {
        if(pkt[idx] == 0) {
            idx++;
            continue;
        }
        uint8_t opt = pkt[idx++];
        if(idx >= len) break;
        uint8_t opt_len = pkt[idx++];
        if(idx + opt_len > len) break;

        switch(opt) {
        case 53: /* Message Type */
            if(opt_len >= 1 && pkt[idx] == DHCP_OFFER) is_offer = true;
            break;
        case 54: /* Server Identifier */
            if(opt_len >= 4) memcpy(server->server_ip, &pkt[idx], 4);
            break;
        case 1: /* Subnet Mask - skip */
            break;
        case 3: /* Router/Gateway */
            if(opt_len >= 4) memcpy(server->gateway, &pkt[idx], 4);
            break;
        case 6: /* DNS */
            if(opt_len >= 4) memcpy(server->dns, &pkt[idx], 4);
            break;
        case 15: /* Domain Name */
            if(opt_len > 0) {
                uint8_t copy = opt_len < sizeof(server->domain) - 1 ? opt_len :
                                                                      sizeof(server->domain) - 1;
                memcpy(server->domain, &pkt[idx], copy);
                server->domain[copy] = '\0';
            }
            break;
        case 51: /* Lease Time */
            if(opt_len >= 4) server->lease_time = pkt_read_u32_be(&pkt[idx]);
            break;
        }
        idx += opt_len;
    }

    return is_offer;
}

/**
 * Find server entry by IP, or create new one.
 */
static RogueDhcpServer* find_or_add_server(RogueDhcpState* state, const uint8_t server_ip[4]) {
    for(uint8_t i = 0; i < state->server_count; i++) {
        if(memcmp(state->servers[i].server_ip, server_ip, 4) == 0) {
            return &state->servers[i];
        }
    }
    if(state->server_count < ROGUE_DHCP_MAX_SERVERS) {
        return &state->servers[state->server_count++];
    }
    return NULL;
}

bool rogue_dhcp_detect(const uint8_t our_mac[6], RogueDhcpState* state, uint32_t listen_ms) {
    memset(state, 0, sizeof(RogueDhcpState));

    close(DHCP_SOCK);
    if(socket(DHCP_SOCK, Sn_MR_UDP, DHCP_CLIENT_PORT, 0) != DHCP_SOCK) return false;

    /* Generate random XID */
    uint32_t xid;
    furi_hal_random_fill_buf((uint8_t*)&xid, 4);

    uint8_t* pkt = malloc(512);
    if(!pkt) {
        close(DHCP_SOCK);
        return false;
    }

    uint16_t pkt_len = build_discover(pkt, 512, our_mac, xid);
    if(pkt_len == 0) {
        free(pkt);
        close(DHCP_SOCK);
        return false;
    }

    /* Send Discover to broadcast */
    uint8_t broadcast[4] = {255, 255, 255, 255};
    sendto(DHCP_SOCK, pkt, pkt_len, broadcast, DHCP_SERVER_PORT);
    state->discover_sent = 1;

    /* Listen for Offers */
    uint32_t start = furi_get_tick();

    while((furi_get_tick() - start) < listen_ms) {
        uint16_t rx_len = getSn_RX_RSR(DHCP_SOCK);
        if(rx_len > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t recv_len = recvfrom(DHCP_SOCK, pkt, sizeof(pkt), from_ip, &from_port);
            if(recv_len > 0) {
                RogueDhcpServer temp;
                memset(&temp, 0, sizeof(temp));
                if(parse_offer(pkt, (uint16_t)recv_len, xid, &temp)) {
                    state->offers_received++;

                    /* Store source MAC from Ethernet header is not available
                     * in UDP mode; use server_ip to identify */
                    RogueDhcpServer* srv = find_or_add_server(state, temp.server_ip);
                    if(srv) {
                        if(srv->offer_count == 0) {
                            *srv = temp;
                        }
                        srv->offer_count++;
                    }
                }
            }
        }
        furi_delay_ms(10);
    }

    free(pkt);
    close(DHCP_SOCK);

    state->multiple_servers = (state->server_count > 1);
    return (state->offers_received > 0);
}
