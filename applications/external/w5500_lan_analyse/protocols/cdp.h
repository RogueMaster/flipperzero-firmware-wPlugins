#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Maximum string length for CDP fields */
#define CDP_MAX_STRING 64

/* CDP destination MAC */
#define CDP_DST_MAC {0x01, 0x00, 0x0C, 0xCC, 0xCC, 0xCC}

/*
 * CDP uses LLC/SNAP encapsulation:
 *   DSAP=0xAA, SSAP=0xAA, Control=0x03
 *   OUI=00-00-0C, Type=0x2000
 */
#define CDP_SNAP_DSAP  0xAA
#define CDP_SNAP_SSAP  0xAA
#define CDP_SNAP_CTRL  0x03
#define CDP_SNAP_OUI_0 0x00
#define CDP_SNAP_OUI_1 0x00
#define CDP_SNAP_OUI_2 0x0C
#define CDP_SNAP_TYPE  0x2000

/* CDP TLV Types */
#define CDP_TLV_DEVICE_ID    0x0001
#define CDP_TLV_ADDRESSES    0x0002
#define CDP_TLV_PORT_ID      0x0003
#define CDP_TLV_CAPABILITIES 0x0004
#define CDP_TLV_SW_VERSION   0x0005
#define CDP_TLV_PLATFORM     0x0006
#define CDP_TLV_VTP_DOMAIN   0x0009
#define CDP_TLV_NATIVE_VLAN  0x000A
#define CDP_TLV_DUPLEX       0x000B

typedef struct {
    char device_id[CDP_MAX_STRING];
    char port_id[CDP_MAX_STRING];
    char platform[CDP_MAX_STRING];
    char sw_version[CDP_MAX_STRING];
    char vtp_domain[CDP_MAX_STRING];
    uint8_t mgmt_ip[4];
    uint16_t native_vlan;
    uint32_t capabilities;
    uint8_t duplex; /* 0 = half, 1 = full */
    uint8_t cdp_version;
    uint8_t cdp_ttl;
    bool valid;
    uint32_t last_seen_tick;
} CdpNeighbor;

/**
 * Check if an Ethernet frame contains CDP (LLC/SNAP encapsulation).
 * frame: complete Ethernet frame (including header)
 * frame_len: total frame length
 * Returns offset to CDP payload, or 0 if not CDP.
 */
uint16_t cdp_check_frame(const uint8_t* frame, uint16_t frame_len);

/**
 * Parse a CDP packet payload.
 * payload: pointer to CDP header (version, TTL, checksum, then TLVs)
 * payload_len: length of CDP data
 * neighbor: output structure
 * Returns true on success.
 */
bool cdp_parse(const uint8_t* payload, uint16_t payload_len, CdpNeighbor* neighbor);

/**
 * Format CDP neighbor info into a human-readable string.
 */
void cdp_format_neighbor(const CdpNeighbor* neighbor, char* buf, uint16_t buf_size);
