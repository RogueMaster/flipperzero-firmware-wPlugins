#pragma once

#include <stdint.h>
#include <stdbool.h>

#define ARP_WATCH_MAX_ENTRIES 64

typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
    uint32_t first_seen_tick;
    uint32_t last_seen_tick;
    uint16_t arp_count; /* number of ARP packets from this IP */
    bool is_duplicate; /* another MAC seen for same IP */
    bool is_gratuitous; /* gratuitous ARP detected */
} ArpWatchEntry;

typedef struct {
    ArpWatchEntry entries[ARP_WATCH_MAX_ENTRIES];
    uint16_t entry_count;
    uint16_t total_arp_seen;
    uint16_t duplicate_count; /* IPs with multiple MACs */
    uint16_t gratuitous_count; /* gratuitous ARP count */
    uint16_t storm_threshold; /* alert if ARP/sec exceeds this */
    bool storm_detected;
} ArpWatchState;

/**
 * Initialize ARP Watch state.
 */
void arp_watch_init(ArpWatchState* state);

/**
 * Process a raw Ethernet frame for ARP analysis.
 * @param state  Watch state
 * @param frame  Raw Ethernet frame
 * @param len    Frame length
 * @return true if anomaly detected (duplicate IP, gratuitous, storm)
 */
bool arp_watch_process_frame(ArpWatchState* state, const uint8_t* frame, uint16_t len);
