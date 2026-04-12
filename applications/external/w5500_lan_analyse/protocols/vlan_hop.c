#include "vlan_hop.h"
#include "../utils/packet_utils.h"
#include "../hal/w5500_hal.h"

#include <furi.h>
#include <string.h>

#define ETH_TYPE_8021Q 0x8100
#define ETH_TYPE_ARP   0x0806

/**
 * Build an 802.1Q tagged ARP Request frame.
 * Ethernet header + 802.1Q tag + ARP payload.
 */
static uint16_t build_tagged_arp_request(
    uint8_t* frame,
    const uint8_t our_mac[6],
    const uint8_t our_ip[4],
    const uint8_t target_ip[4],
    uint16_t vlan_id) {
    uint16_t idx = 0;

    /* Destination: broadcast */
    memset(&frame[idx], 0xFF, 6);
    idx += 6;
    /* Source: our MAC */
    memcpy(&frame[idx], our_mac, 6);
    idx += 6;

    /* 802.1Q Tag: TPID=0x8100, TCI = priority(3) + DEI(1) + VLAN ID(12) */
    pkt_write_u16_be(&frame[idx], ETH_TYPE_8021Q);
    idx += 2;
    uint16_t tci = vlan_id & 0x0FFF; /* priority=0, DEI=0 */
    pkt_write_u16_be(&frame[idx], tci);
    idx += 2;

    /* EtherType: ARP */
    pkt_write_u16_be(&frame[idx], ETH_TYPE_ARP);
    idx += 2;

    /* ARP header (28 bytes) */
    pkt_write_u16_be(&frame[idx], 0x0001); /* HTYPE: Ethernet */
    idx += 2;
    pkt_write_u16_be(&frame[idx], 0x0800); /* PTYPE: IPv4 */
    idx += 2;
    frame[idx++] = 6; /* HLEN */
    frame[idx++] = 4; /* PLEN */
    pkt_write_u16_be(&frame[idx], 0x0001); /* OPER: Request */
    idx += 2;
    memcpy(&frame[idx], our_mac, 6); /* Sender MAC */
    idx += 6;
    memcpy(&frame[idx], our_ip, 4); /* Sender IP */
    idx += 4;
    memset(&frame[idx], 0x00, 6); /* Target MAC: unknown */
    idx += 6;
    memcpy(&frame[idx], target_ip, 4); /* Target IP */
    idx += 4;

    return idx; /* 46 bytes total (18 eth+tag + 28 ARP) */
}

bool vlan_hop_test(
    const uint8_t our_mac[6],
    const uint8_t our_ip[4],
    const uint8_t target_ip[4],
    uint16_t vlan_id,
    VlanHopResult* result) {
    memset(result, 0, sizeof(VlanHopResult));
    result->test_vlan_id = vlan_id;

    if(!w5500_hal_open_macraw()) return false;

    /* Build tagged ARP request */
    uint8_t frame[64];
    uint16_t frame_len = build_tagged_arp_request(frame, our_mac, our_ip, target_ip, vlan_id);

    /* Send tagged frame 3 times */
    for(int i = 0; i < 3; i++) {
        w5500_hal_macraw_send(frame, frame_len);
        furi_delay_ms(200);
    }

    /* Listen for replies */
    uint32_t start = furi_get_tick();
    uint32_t timeout_ms = 5000;
    uint8_t rx_buf[256];

    while((furi_get_tick() - start) < timeout_ms) {
        uint16_t recv_len = w5500_hal_macraw_recv(rx_buf, sizeof(rx_buf));
        if(recv_len < 14) {
            furi_delay_ms(5);
            continue;
        }

        uint16_t ethertype = pkt_get_ethertype(rx_buf);

        /* Check for 802.1Q tagged frames */
        if(ethertype == ETH_TYPE_8021Q && recv_len >= 18) {
            result->tagged_frames_seen++;
            uint16_t tci = pkt_read_u16_be(&rx_buf[14]);
            uint16_t rx_vlan = tci & 0x0FFF;
            uint16_t inner_type = pkt_read_u16_be(&rx_buf[16]);

            if(inner_type == ETH_TYPE_ARP && rx_vlan == vlan_id) {
                /* Got ARP reply on the test VLAN */
                const uint8_t* arp = &rx_buf[18];
                if(recv_len >= 18 + 28) {
                    uint16_t oper = pkt_read_u16_be(&arp[6]);
                    if(oper == 2) { /* ARP Reply */
                        /* Check if it's directed at us */
                        if(memcmp(&arp[24], our_ip, 4) == 0) {
                            result->tagged_reply = true;
                        }
                    }
                }
            }
        }

        /* Check for untagged ARP replies to our request */
        if(ethertype == ETH_TYPE_ARP && recv_len >= 42) {
            result->untagged_frames_seen++;
            const uint8_t* arp = &rx_buf[14];
            uint16_t oper = pkt_read_u16_be(&arp[6]);
            if(oper == 2) { /* ARP Reply */
                if(memcmp(&arp[24], our_ip, 4) == 0) {
                    result->native_reply = true;
                }
            }
        }

        furi_delay_ms(5);
    }

    w5500_hal_close_macraw();

    /* Interpretation:
     * - tagged_reply=true: VLAN isolation FAILED (our tagged frame crossed VLANs)
     * - native_reply=true with no tagged_reply: switch might be stripping tags
     * - no reply at all: VLAN isolation likely works (or target is unreachable)
     */
    result->isolation_ok = !result->tagged_reply;
    result->valid = true;

    return true;
}
