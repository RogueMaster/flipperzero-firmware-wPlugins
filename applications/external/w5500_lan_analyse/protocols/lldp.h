#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Maximum string length for LLDP fields */
#define LLDP_MAX_STRING 64

/* LLDP EtherType */
#define ETHERTYPE_LLDP 0x88CC

/* LLDP multicast destination MAC */
#define LLDP_DST_MAC {0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E}

/* LLDP TLV Types (IEEE 802.1AB) */
#define LLDP_TLV_END          0
#define LLDP_TLV_CHASSIS_ID   1
#define LLDP_TLV_PORT_ID      2
#define LLDP_TLV_TTL          3
#define LLDP_TLV_PORT_DESC    4
#define LLDP_TLV_SYSTEM_NAME  5
#define LLDP_TLV_SYSTEM_DESC  6
#define LLDP_TLV_SYSTEM_CAP   7
#define LLDP_TLV_MGMT_ADDR    8
#define LLDP_TLV_ORG_SPECIFIC 127

/* Chassis ID subtypes */
#define LLDP_CHASSIS_SUBTYPE_MAC 4

/* System capabilities bits */
#define LLDP_CAP_OTHER     (1 << 0)
#define LLDP_CAP_REPEATER  (1 << 1)
#define LLDP_CAP_BRIDGE    (1 << 2)
#define LLDP_CAP_WLAN_AP   (1 << 3)
#define LLDP_CAP_ROUTER    (1 << 4)
#define LLDP_CAP_TELEPHONE (1 << 5)
#define LLDP_CAP_DOCSIS    (1 << 6)
#define LLDP_CAP_STATION   (1 << 7)

typedef struct {
    char system_name[LLDP_MAX_STRING];
    char port_id[LLDP_MAX_STRING];
    char port_desc[LLDP_MAX_STRING];
    char system_desc[LLDP_MAX_STRING];
    uint8_t chassis_mac[6];
    uint8_t mgmt_ip[4];
    uint16_t mgmt_vlan;
    uint16_t ttl;
    uint16_t capabilities;
    uint16_t enabled_capabilities;
    bool valid;
    uint32_t last_seen_tick;
} LldpNeighbor;

/**
 * Parse an LLDP frame (starting after the Ethernet header, i.e. after 14 bytes).
 * payload: pointer to the LLDP PDU (after EtherType)
 * payload_len: length of the LLDP PDU
 * neighbor: output structure to fill
 * Returns true if parsing succeeded.
 */
bool lldp_parse(const uint8_t* payload, uint16_t payload_len, LldpNeighbor* neighbor);

/**
 * Format LLDP neighbor info into a human-readable string.
 * buf: output buffer
 * buf_size: buffer size
 */
void lldp_format_neighbor(const LldpNeighbor* neighbor, char* buf, uint16_t buf_size);
