#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ROGUE_RA_MAX_ROUTERS 8

typedef struct {
    uint8_t src_mac[6];
    uint8_t src_ip[16]; /* IPv6 source address */
    uint8_t prefix[16]; /* advertised prefix */
    uint8_t prefix_len;
    uint8_t cur_hop_limit;
    uint16_t router_lifetime;
    bool managed_flag; /* M flag: managed address config */
    bool other_flag; /* O flag: other config */
    uint32_t reachable_time;
    uint32_t retrans_timer;
} RogueRaRouter;

typedef struct {
    RogueRaRouter routers[ROGUE_RA_MAX_ROUTERS];
    uint8_t router_count;
    uint16_t total_ra_seen;
    bool multiple_routers; /* true if >1 unique RA source */
} RogueRaState;

/**
 * Initialize Rogue RA detection state.
 */
void rogue_ra_init(RogueRaState* state);

/**
 * Process a raw Ethernet frame for IPv6 Router Advertisement.
 * @param state  RA detection state
 * @param frame  Raw Ethernet frame
 * @param len    Frame length
 * @return true if a new RA router was detected
 */
bool rogue_ra_process_frame(RogueRaState* state, const uint8_t* frame, uint16_t len);
