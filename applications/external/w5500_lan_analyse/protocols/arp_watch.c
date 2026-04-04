#include "arp_watch.h"
#include "../utils/packet_utils.h"
#include <string.h>

#define ETH_TYPE_ARP   0x0806
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

void arp_watch_init(ArpWatchState* state) {
    memset(state, 0, sizeof(ArpWatchState));
    state->storm_threshold = 50; /* ARP packets per scan cycle = storm */
}

/**
 * Find or create entry for an IP address.
 */
static ArpWatchEntry* arp_watch_find_or_create(ArpWatchState* state, const uint8_t ip[4]) {
    /* Search existing */
    for(uint16_t i = 0; i < state->entry_count; i++) {
        if(memcmp(state->entries[i].ip, ip, 4) == 0) {
            return &state->entries[i];
        }
    }
    /* Create new if space available */
    if(state->entry_count < ARP_WATCH_MAX_ENTRIES) {
        ArpWatchEntry* e = &state->entries[state->entry_count];
        memset(e, 0, sizeof(ArpWatchEntry));
        memcpy(e->ip, ip, 4);
        state->entry_count++;
        return e;
    }
    return NULL;
}

bool arp_watch_process_frame(ArpWatchState* state, const uint8_t* frame, uint16_t len) {
    if(len < 42) return false; /* minimum ARP frame = 14 eth + 28 ARP */

    uint16_t ethertype = pkt_get_ethertype(frame);
    if(ethertype != ETH_TYPE_ARP) return false;

    /* ARP header starts at offset 14 */
    const uint8_t* arp = frame + 14;

    /* Validate ARP: HTYPE=1 (Ethernet), PTYPE=0x0800 (IPv4), HLEN=6, PLEN=4 */
    if(pkt_read_u16_be(&arp[0]) != 0x0001) return false;
    if(pkt_read_u16_be(&arp[2]) != 0x0800) return false;
    if(arp[4] != 6 || arp[5] != 4) return false;

    uint16_t opcode = pkt_read_u16_be(&arp[6]);
    const uint8_t* sender_mac = &arp[8];
    const uint8_t* sender_ip = &arp[14];
    const uint8_t* target_ip = &arp[24];

    state->total_arp_seen++;
    bool anomaly = false;

    /* Skip 0.0.0.0 sender (DHCP probing) */
    if(sender_ip[0] == 0 && sender_ip[1] == 0 && sender_ip[2] == 0 && sender_ip[3] == 0) {
        return false;
    }

    /* Check for gratuitous ARP: sender IP == target IP */
    bool is_gratuitous = (memcmp(sender_ip, target_ip, 4) == 0);

    ArpWatchEntry* entry = arp_watch_find_or_create(state, sender_ip);
    if(!entry) return false;

    uint32_t now = 0; /* Caller tracks time; we just count */
    (void)now;

    if(entry->arp_count == 0) {
        /* First time seeing this IP */
        memcpy(entry->mac, sender_mac, 6);
        entry->arp_count = 1;
    } else {
        entry->arp_count++;
        /* Check for IP spoofing: different MAC for same IP */
        if(memcmp(entry->mac, sender_mac, 6) != 0) {
            if(!entry->is_duplicate) {
                entry->is_duplicate = true;
                state->duplicate_count++;
                anomaly = true;
            }
        }
    }

    if(is_gratuitous && (opcode == ARP_OP_REPLY || opcode == ARP_OP_REQUEST)) {
        if(!entry->is_gratuitous) {
            entry->is_gratuitous = true;
            state->gratuitous_count++;
            anomaly = true;
        }
    }

    /* Storm detection */
    if(state->total_arp_seen > state->storm_threshold) {
        state->storm_detected = true;
        anomaly = true;
    }

    return anomaly;
}
