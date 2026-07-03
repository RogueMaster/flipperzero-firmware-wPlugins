#pragma once

#include <stdint.h>
#include <stdbool.h>

/* STP/BPDU destination MAC */
#define STP_DST_MAC {0x01, 0x80, 0xC2, 0x00, 0x00, 0x00}

/* 802.1Q TPID */
#define ETHERTYPE_8021Q 0x8100

/* BPDU LLC/SNAP header */
#define BPDU_LLC_DSAP 0x42
#define BPDU_LLC_SSAP 0x42

/* Max unique VLANs to track */
#define MAX_VLANS 64

/* BPDU parsed info */
typedef struct {
    uint8_t protocol_id; /* Should be 0x0000 for STP */
    uint8_t version; /* 0=STP, 2=RSTP, 3=MSTP */
    uint8_t type; /* 0x00=Config, 0x02=TCN, 0x80=RST */
    uint8_t flags;
    uint8_t root_bridge_id[8]; /* Priority(2) + MAC(6) */
    uint32_t root_path_cost;
    uint8_t sender_bridge_id[8]; /* Priority(2) + MAC(6) */
    uint16_t port_id;
    uint16_t message_age;
    uint16_t max_age;
    uint16_t hello_time;
    uint16_t forward_delay;
    bool valid;
    bool topology_change;
} BpduInfo;

/* VLAN tracking entry */
typedef struct {
    uint16_t vlan_id;
    uint32_t frame_count;
} VlanEntry;

/* VLAN detection state */
typedef struct {
    VlanEntry vlans[MAX_VLANS];
    uint16_t vlan_count;
    uint32_t total_tagged_frames;
} VlanState;

/**
 * Parse a BPDU from a raw Ethernet frame.
 * frame: complete Ethernet frame
 * frame_len: frame length
 * info: output
 * Returns true if valid BPDU parsed.
 */
bool stp_parse_bpdu(const uint8_t* frame, uint16_t frame_len, BpduInfo* info);

/**
 * Format BPDU info as human-readable string.
 */
void stp_format_bpdu(const BpduInfo* info, char* buf, uint16_t buf_size);

/**
 * Check if a frame has an 802.1Q VLAN tag and extract the VLAN ID.
 * frame: complete Ethernet frame
 * frame_len: frame length
 * vlan_id: output VLAN ID (12-bit)
 * Returns true if 802.1Q tag found.
 */
bool vlan_extract_tag(const uint8_t* frame, uint16_t frame_len, uint16_t* vlan_id);

/**
 * Add a VLAN ID to the tracking state.
 */
void vlan_state_add(VlanState* state, uint16_t vlan_id);

/**
 * Initialize VLAN state.
 */
void vlan_state_init(VlanState* state);
