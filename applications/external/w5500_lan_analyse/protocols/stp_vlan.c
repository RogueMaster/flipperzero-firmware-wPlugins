#include "stp_vlan.h"

#include <string.h>
#include <stdio.h>

bool stp_parse_bpdu(const uint8_t* frame, uint16_t frame_len, BpduInfo* info) {
    memset(info, 0, sizeof(BpduInfo));

    if(frame_len < 14) return false;

    /* Check destination MAC is STP multicast */
    const uint8_t stp_mac[] = STP_DST_MAC;
    if(memcmp(frame, stp_mac, 6) != 0) return false;

    /* Check for LLC header (DSAP=0x42, SSAP=0x42, Control=0x03) */
    /* EtherType/Length field at offset 12-13 */
    uint16_t len_or_type = ((uint16_t)frame[12] << 8) | frame[13];

    uint16_t bpdu_offset;
    if(len_or_type <= 0x05DC) {
        /* IEEE 802.3 frame with LLC */
        if(frame_len < 17) return false;
        if(frame[14] != BPDU_LLC_DSAP || frame[15] != BPDU_LLC_SSAP) return false;
        /* Control byte at [16] = 0x03 (UI) */
        bpdu_offset = 17;
    } else {
        /* Could be raw BPDU (some implementations) */
        bpdu_offset = 14;
    }

    /* Need at least 4 bytes for BPDU header (protocol_id, version, type) */
    if(bpdu_offset + 4 > frame_len) return false;

    /* Parse BPDU header */
    info->protocol_id = frame[bpdu_offset]; /* Should be 0 */
    info->version = frame[bpdu_offset + 2];
    info->type = frame[bpdu_offset + 3];

    /* For Config BPDU (type 0x00) or RST BPDU (type 0x02), parse full fields */
    if(info->type == 0x00 || info->type == 0x02) {
        if(bpdu_offset + 35 > frame_len) {
            /* Minimal BPDU, just mark as valid with what we have */
            info->valid = true;
            return true;
        }

        info->flags = frame[bpdu_offset + 4];
        memcpy(info->root_bridge_id, &frame[bpdu_offset + 5], 8);
        info->root_path_cost = ((uint32_t)frame[bpdu_offset + 13] << 24) |
                               ((uint32_t)frame[bpdu_offset + 14] << 16) |
                               ((uint32_t)frame[bpdu_offset + 15] << 8) | frame[bpdu_offset + 16];
        memcpy(info->sender_bridge_id, &frame[bpdu_offset + 17], 8);
        info->port_id = ((uint16_t)frame[bpdu_offset + 25] << 8) | frame[bpdu_offset + 26];
        info->message_age = ((uint16_t)frame[bpdu_offset + 27] << 8) | frame[bpdu_offset + 28];
        info->max_age = ((uint16_t)frame[bpdu_offset + 29] << 8) | frame[bpdu_offset + 30];
        info->hello_time = ((uint16_t)frame[bpdu_offset + 31] << 8) | frame[bpdu_offset + 32];
        info->forward_delay = ((uint16_t)frame[bpdu_offset + 33] << 8) | frame[bpdu_offset + 34];

        info->topology_change = (info->flags & 0x01) != 0;
    }

    info->valid = true;
    return true;
}

void stp_format_bpdu(const BpduInfo* info, char* buf, uint16_t buf_size) {
    /* Format root bridge MAC from bridge ID (last 6 bytes) */
    const uint8_t* root_mac = &info->root_bridge_id[2];
    uint16_t root_pri = ((uint16_t)info->root_bridge_id[0] << 8) | info->root_bridge_id[1];

    const uint8_t* sender_mac = &info->sender_bridge_id[2];
    uint16_t sender_pri = ((uint16_t)info->sender_bridge_id[0] << 8) | info->sender_bridge_id[1];

    const char* ver_str;
    switch(info->version) {
    case 0:
        ver_str = "STP";
        break;
    case 2:
        ver_str = "RSTP";
        break;
    case 3:
        ver_str = "MSTP";
        break;
    default:
        ver_str = "?";
        break;
    }

    snprintf(
        buf,
        buf_size,
        "=== BPDU (%s) ===\n"
        "Root: %02X:%02X:%02X:%02X:%02X:%02X\n"
        " Pri: %d  Cost: %lu\n"
        "Sender: %02X:%02X:%02X:%02X:%02X:%02X\n"
        " Pri: %d  Port: 0x%04X\n"
        "Hello: %ds  MaxAge: %ds\n"
        "FwdDelay: %ds\n"
        "TC: %s\n",
        ver_str,
        root_mac[0],
        root_mac[1],
        root_mac[2],
        root_mac[3],
        root_mac[4],
        root_mac[5],
        root_pri,
        (unsigned long)info->root_path_cost,
        sender_mac[0],
        sender_mac[1],
        sender_mac[2],
        sender_mac[3],
        sender_mac[4],
        sender_mac[5],
        sender_pri,
        info->port_id,
        info->hello_time / 256,
        info->max_age / 256,
        info->forward_delay / 256,
        info->topology_change ? "YES" : "No");
}

bool vlan_extract_tag(const uint8_t* frame, uint16_t frame_len, uint16_t* vlan_id) {
    if(frame_len < 18) return false;

    /* Check for 802.1Q TPID at offset 12 */
    uint16_t tpid = ((uint16_t)frame[12] << 8) | frame[13];
    if(tpid != ETHERTYPE_8021Q) return false;

    /* VLAN ID is in the lower 12 bits of TCI at offset 14-15 */
    uint16_t tci = ((uint16_t)frame[14] << 8) | frame[15];
    *vlan_id = tci & 0x0FFF;
    return true;
}

void vlan_state_init(VlanState* state) {
    memset(state, 0, sizeof(VlanState));
}

void vlan_state_add(VlanState* state, uint16_t vlan_id) {
    state->total_tagged_frames++;

    /* Check if VLAN already tracked */
    for(uint16_t i = 0; i < state->vlan_count; i++) {
        if(state->vlans[i].vlan_id == vlan_id) {
            state->vlans[i].frame_count++;
            return;
        }
    }

    /* Add new VLAN */
    if(state->vlan_count < MAX_VLANS) {
        state->vlans[state->vlan_count].vlan_id = vlan_id;
        state->vlans[state->vlan_count].frame_count = 1;
        state->vlan_count++;
    }
}
