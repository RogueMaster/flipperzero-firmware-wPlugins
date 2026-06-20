#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t test_vlan_id; /* VLAN ID used for test */
    bool tagged_reply; /* received any reply to tagged frame */
    bool native_reply; /* received reply on native (untagged) */
    bool isolation_ok; /* true if VLAN isolation appears correct */
    uint16_t tagged_frames_seen; /* 802.1Q frames seen during test */
    uint16_t untagged_frames_seen;
    bool valid;
} VlanHopResult;

/**
 * VLAN hopping test: send 802.1Q tagged frame and check for response.
 * Tests if VLAN isolation is properly enforced on the switch port.
 * @param our_mac   Our MAC address
 * @param our_ip    Our IP address
 * @param target_ip Target IP (usually gateway)
 * @param vlan_id   VLAN ID to test (1-4094)
 * @param result    Output result
 * @return true if test completed
 */
bool vlan_hop_test(
    const uint8_t our_mac[6],
    const uint8_t our_ip[4],
    const uint8_t target_ip[4],
    uint16_t vlan_id,
    VlanHopResult* result);
