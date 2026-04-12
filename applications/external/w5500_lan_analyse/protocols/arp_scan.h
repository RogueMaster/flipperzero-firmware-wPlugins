#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ARP EtherType */
#define ETHERTYPE_ARP 0x0806

/* ARP operations */
#define ARP_OP_REQUEST 0x0001
#define ARP_OP_REPLY   0x0002

/* ARP hardware/protocol types */
#define ARP_HTYPE_ETHERNET 0x0001
#define ARP_PTYPE_IPV4     0x0800
#define ARP_HLEN_ETHERNET  6
#define ARP_PLEN_IPV4      4

/* Maximum discovered hosts (hard cap for RAM: 128 × 16 = 2 KB) */
#define ARP_MAX_HOSTS_CAP 128

/* Batch size for sending ARP requests (keep low to stay under switch DAI limits) */
#define ARP_BATCH_SIZE 2

/* Delay between batches in ms (~4 pps, safe for Cisco DAI default 15 pps) */
#define ARP_BATCH_DELAY_MS 500

/* Wait time after all requests sent (ms) */
#define ARP_TAIL_WAIT_MS 3000

typedef struct {
    uint8_t ip[4];
    uint8_t mac[6];
    const char* vendor; /* pointer to OUI string in flash (not copied) */
    bool responded;
} ArpHost;

typedef struct {
    ArpHost* hosts; /* heap-allocated, capacity = max_hosts */
    uint16_t max_hosts; /* allocated capacity */
    uint16_t count;
    uint16_t total_sent;
    uint8_t progress_percent;
    bool scanning;
    bool complete;
    uint32_t start_tick;
    uint32_t elapsed_ms;
} ArpScanState;

/**
 * Build an ARP request frame.
 * buf: output buffer (must be at least 42 bytes)
 * src_mac: our MAC address
 * src_ip: our IP address
 * target_ip: IP to resolve
 * Returns frame length (always 42 bytes for ARP).
 */
uint16_t arp_build_request(
    uint8_t* buf,
    const uint8_t src_mac[6],
    const uint8_t src_ip[4],
    const uint8_t target_ip[4]);

/**
 * Parse an ARP reply from a raw Ethernet frame.
 * frame: complete Ethernet frame
 * frame_len: frame length
 * sender_mac: output sender MAC (6 bytes)
 * sender_ip: output sender IP (4 bytes)
 * Returns true if this is a valid ARP reply.
 */
bool arp_parse_reply(
    const uint8_t* frame,
    uint16_t frame_len,
    uint8_t sender_mac[6],
    uint8_t sender_ip[4]);

/**
 * Calculate the host range from IP and subnet mask.
 * ip: our IP
 * mask: subnet mask
 * start_ip: output, first host IP in range
 * end_ip: output, last host IP in range
 * Returns number of hosts to scan (0 if point-to-point/host-only).
 */
uint16_t arp_calc_scan_range(
    const uint8_t ip[4],
    const uint8_t mask[4],
    uint8_t start_ip[4],
    uint8_t end_ip[4]);

/**
 * Calculate CIDR prefix length from subnet mask.
 * Returns 0..32.
 */
uint8_t arp_mask_to_prefix(const uint8_t mask[4]);
