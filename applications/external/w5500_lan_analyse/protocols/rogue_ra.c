#include "rogue_ra.h"
#include "../utils/packet_utils.h"
#include <string.h>

#define ETH_TYPE_IPV6    0x86DD
#define IPV6_NEXT_ICMPV6 58
#define ICMPV6_RA_TYPE   134

/* ICMPv6 RA option types */
#define ND_OPT_PREFIX_INFO 3

void rogue_ra_init(RogueRaState* state) {
    memset(state, 0, sizeof(RogueRaState));
}

/**
 * Check if a router already seen (by MAC).
 */
static RogueRaRouter* rogue_ra_find_router(RogueRaState* state, const uint8_t mac[6]) {
    for(uint8_t i = 0; i < state->router_count; i++) {
        if(memcmp(state->routers[i].src_mac, mac, 6) == 0) {
            return &state->routers[i];
        }
    }
    return NULL;
}

bool rogue_ra_process_frame(RogueRaState* state, const uint8_t* frame, uint16_t len) {
    if(len < 14) return false;

    uint16_t ethertype = pkt_get_ethertype(frame);
    if(ethertype != ETH_TYPE_IPV6) return false;

    /* IPv6 header starts at offset 14, minimum 40 bytes */
    if(len < 14 + 40) return false;
    const uint8_t* ipv6 = frame + 14;

    /* Check version (4 bits) */
    if((ipv6[0] >> 4) != 6) return false;

    /* Next header */
    uint8_t next_header = ipv6[6];
    /* Payload length */
    uint16_t payload_len = pkt_read_u16_be(&ipv6[4]);

    /* Source IPv6 address: bytes 8-23 */
    const uint8_t* src_ipv6 = &ipv6[8];

    /* Source MAC from Ethernet header */
    uint8_t src_mac[6];
    pkt_get_src_mac(frame, src_mac);

    /* Handle extension headers - skip to ICMPv6 */
    const uint8_t* payload = ipv6 + 40;
    uint16_t remaining = (len - 14 - 40 < payload_len) ? len - 14 - 40 : payload_len;

    /* Simple: only handle direct ICMPv6 (no extension headers) */
    if(next_header != IPV6_NEXT_ICMPV6) return false;

    /* ICMPv6 header: type(1) + code(1) + checksum(2) */
    if(remaining < 4) return false;
    uint8_t icmpv6_type = payload[0];

    if(icmpv6_type != ICMPV6_RA_TYPE) return false;

    /* Router Advertisement: 16 bytes minimum after ICMPv6 header
     * type(1) + code(1) + cksum(2) + cur_hop(1) + flags(1) + lifetime(2) +
     * reachable(4) + retrans(4) = 16 bytes */
    if(remaining < 16) return false;

    state->total_ra_seen++;

    /* Check if we already know this router */
    RogueRaRouter* existing = rogue_ra_find_router(state, src_mac);
    if(existing) return false; /* Already known, not a new detection */

    /* Add new router */
    if(state->router_count >= ROGUE_RA_MAX_ROUTERS) return false;

    RogueRaRouter* router = &state->routers[state->router_count];
    memset(router, 0, sizeof(RogueRaRouter));
    memcpy(router->src_mac, src_mac, 6);
    memcpy(router->src_ip, src_ipv6, 16);

    uint8_t flags = payload[5];
    router->managed_flag = (flags & 0x80) != 0;
    router->other_flag = (flags & 0x40) != 0;
    router->router_lifetime = pkt_read_u16_be(&payload[6]);

    /* Parse options to find Prefix Information */
    uint16_t opt_offset = 16;
    while(opt_offset + 2 <= remaining) {
        uint8_t opt_type = payload[opt_offset];
        uint8_t opt_len_units = payload[opt_offset + 1]; /* in 8-byte units */
        if(opt_len_units == 0) break;
        uint16_t opt_len_bytes = opt_len_units * 8;

        if(opt_type == ND_OPT_PREFIX_INFO && opt_len_bytes >= 32 && opt_offset + 32 <= remaining) {
            router->prefix_len = payload[opt_offset + 2];
            /* Prefix starts at offset +16 within the option */
            memcpy(router->prefix, &payload[opt_offset + 16], 16);
        }

        opt_offset += opt_len_bytes;
    }

    state->router_count++;
    state->multiple_routers = (state->router_count > 1);
    return true;
}
