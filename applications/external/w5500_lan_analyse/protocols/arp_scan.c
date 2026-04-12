#include "arp_scan.h"
#include "../utils/packet_utils.h"

#include <furi.h>
#include <string.h>

#define TAG "ARP"

uint16_t arp_build_request(
    uint8_t* buf,
    const uint8_t src_mac[6],
    const uint8_t src_ip[4],
    const uint8_t target_ip[4]) {
    furi_assert(buf);

    /* Ethernet header */
    /* Destination: broadcast FF:FF:FF:FF:FF:FF */
    memset(buf, 0xFF, 6);
    /* Source: our MAC */
    memcpy(buf + 6, src_mac, 6);
    /* EtherType: ARP (0x0806) */
    pkt_write_u16_be(buf + 12, ETHERTYPE_ARP);

    /* ARP payload starts at offset 14 */
    uint8_t* arp = buf + ETH_HEADER_SIZE;

    /* Hardware type: Ethernet (1) */
    pkt_write_u16_be(arp + 0, ARP_HTYPE_ETHERNET);
    /* Protocol type: IPv4 (0x0800) */
    pkt_write_u16_be(arp + 2, ARP_PTYPE_IPV4);
    /* Hardware address length: 6 */
    arp[4] = ARP_HLEN_ETHERNET;
    /* Protocol address length: 4 */
    arp[5] = ARP_PLEN_IPV4;
    /* Operation: Request (1) */
    pkt_write_u16_be(arp + 6, ARP_OP_REQUEST);

    /* Sender hardware address (SHA): our MAC */
    memcpy(arp + 8, src_mac, 6);
    /* Sender protocol address (SPA): our IP */
    memcpy(arp + 14, src_ip, 4);
    /* Target hardware address (THA): zeros (unknown) */
    memset(arp + 18, 0, 6);
    /* Target protocol address (TPA): target IP */
    memcpy(arp + 24, target_ip, 4);

    /* Total: 14 (Ethernet) + 28 (ARP) = 42 bytes */
    return 42;
}

bool arp_parse_reply(
    const uint8_t* frame,
    uint16_t frame_len,
    uint8_t sender_mac[6],
    uint8_t sender_ip[4]) {
    /* Minimum ARP frame: 14 (Ethernet) + 28 (ARP) = 42 bytes */
    if(frame_len < 42) return false;

    /* Check EtherType */
    uint16_t ethertype = pkt_get_ethertype(frame);
    if(ethertype != ETHERTYPE_ARP) return false;

    const uint8_t* arp = frame + ETH_HEADER_SIZE;

    /* Check HTYPE = Ethernet */
    if(pkt_read_u16_be(arp + 0) != ARP_HTYPE_ETHERNET) return false;
    /* Check PTYPE = IPv4 */
    if(pkt_read_u16_be(arp + 2) != ARP_PTYPE_IPV4) return false;
    /* Check HLEN = 6, PLEN = 4 */
    if(arp[4] != ARP_HLEN_ETHERNET || arp[5] != ARP_PLEN_IPV4) return false;
    /* Check Operation = Reply (2) */
    if(pkt_read_u16_be(arp + 6) != ARP_OP_REPLY) return false;

    /* Extract sender hardware address (SHA) and sender protocol address (SPA) */
    memcpy(sender_mac, arp + 8, 6);
    memcpy(sender_ip, arp + 14, 4);

    return true;
}

uint16_t arp_calc_scan_range(
    const uint8_t ip[4],
    const uint8_t mask[4],
    uint8_t start_ip[4],
    uint8_t end_ip[4]) {
    /* Calculate network address and broadcast address */
    uint32_t ip_addr = pkt_read_u32_be(ip);
    uint32_t mask_addr = pkt_read_u32_be(mask);
    uint32_t network = ip_addr & mask_addr;
    uint32_t broadcast = network | ~mask_addr;

    /* First host = network + 1, Last host = broadcast - 1 */
    uint32_t first_host = network + 1;
    uint32_t last_host = broadcast - 1;

    uint32_t num_hosts = last_host - first_host + 1;
    if(num_hosts == 0 || first_host >= last_host) {
        return 0;
    }

    /* Cap to uint16_t max (65535) */
    if(num_hosts > 0xFFFF) {
        num_hosts = 0xFFFF;
        last_host = first_host + num_hosts - 1;
    }

    /* Write start and end IPs */
    pkt_write_u32_be(start_ip, first_host);
    pkt_write_u32_be(end_ip, last_host);

    FURI_LOG_I("ARP", "Scan range: %lu hosts", (unsigned long)num_hosts);
    return (uint16_t)num_hosts;
}

uint8_t arp_mask_to_prefix(const uint8_t mask[4]) {
    uint32_t m = pkt_read_u32_be(mask);
    uint8_t bits = 0;
    while(m & 0x80000000) {
        bits++;
        m <<= 1;
    }
    return bits;
}
