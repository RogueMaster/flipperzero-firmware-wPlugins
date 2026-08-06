#include "udp.h"
#include "ipv4.h"
#include "ethernet_protocol.h"

typedef struct {
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
    uint8_t zeros;
    uint8_t protocol;
    uint8_t length[2];
} ip_pseudo_header_t;

uint16_t udp_calculate_checksum(
    ipv4_header_t* ip_header,
    udp_header_t* udp_header,
    uint8_t* data,
    uint16_t len) {
    uint16_t total_len = sizeof(udp_header_t) + len;
    uint8_t buffer[1518]; // Temporary buffer
    ip_pseudo_header_t* pseudo_header = (ip_pseudo_header_t*)buffer;

    // Create pseudo header
    memcpy(pseudo_header->src_ip, ip_header->source_ip, 4);
    memcpy(pseudo_header->dst_ip, ip_header->dest_ip, 4);
    pseudo_header->zeros = 0;
    pseudo_header->protocol = 17; // UDP
    pseudo_header->length[0] = total_len >> 8;
    pseudo_header->length[1] = (uint8_t)total_len;

    // Copy UDP header and data
    memcpy(buffer + sizeof(ip_pseudo_header_t), udp_header, sizeof(udp_header_t));
    memcpy(buffer + sizeof(ip_pseudo_header_t) + sizeof(udp_header_t), data, len);

    // If length is odd, pad with zero
    if(len % 2) {
        buffer[sizeof(ip_pseudo_header_t) + sizeof(udp_header_t) + len] = 0;
        total_len++;
    }

    return calculate_checksum(buffer, sizeof(ip_pseudo_header_t) + total_len);
}

bool set_udp_header(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t destination_port,
    uint16_t length) {
    if(!buffer) return false;

    udp_header_t* header = (udp_header_t*)buffer;

    header->dest_port[0] = (destination_port >> 8) & 0xff;
    header->dest_port[1] = destination_port & 0xff;

    header->source_port[0] = (source_port >> 8) & 0xff;
    header->source_port[1] = source_port & 0xff;

    // Set lenght of the Message
    header->length[0] = (length >> 8) & 0xff;
    header->length[1] = length & 0xff;

    // Checksum set in zeros (in IPV4 is optional)
    header->checksum[0] = 0;
    header->checksum[1] = 0;

    return true;
}

udp_header_t udp_get_header(uint8_t* buffer) {
    udp_header_t udp_header = {0};

    memcpy((uint8_t*)&udp_header, buffer + 34, UDP_HEADER_LEN);

    return udp_header;
}

bool is_udp(uint8_t* buffer) {
    if(!is_ipv4(buffer)) return false;

    ipv4_header_t ip_header = ipv4_get_header(buffer);

    return ip_header.protocol == 0x11;
}

// Create a complete UDP packet with Ethernet, IP, and UDP headers
bool create_udp_packet(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t src_port,
    uint16_t dst_port,
    uint8_t* payload,
    uint16_t payload_length) {
    if(!buffer || !src_mac || !dst_mac || !src_ip || !dst_ip) {
        return false;
    }

    // Set Ethernet header (type 0x0800 for IPv4)
    if(!set_ethernet_header(buffer, src_mac, dst_mac, 0x0800)) {
        return false;
    }

    // Set IP header (protocol 17 for UDP)
    uint8_t* ip_header_ptr = buffer + ETHERNET_HEADER_LEN;
    uint16_t total_udp_length = UDP_HEADER_LEN + payload_length;

    if(!set_ipv4_header(ip_header_ptr, 17, total_udp_length, src_ip, dst_ip, 0, 0x4000, WIN_TTL)) {
        return false;
    }

    // Set UDP header
    uint8_t* udp_header_ptr = buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN;

    if(!set_udp_header(udp_header_ptr, src_port, dst_port, total_udp_length)) {
        return false;
    }

    // Copy payload data if any
    if(payload_length > 0 && payload != NULL) {
        memcpy(udp_header_ptr + UDP_HEADER_LEN, payload, payload_length);
    }

    // Calculate and set UDP checksum (optional in IPv4 but recommended)
    ipv4_header_t* ip_header = (ipv4_header_t*)ip_header_ptr;
    udp_header_t* udp_header = (udp_header_t*)udp_header_ptr;

    uint16_t checksum = udp_calculate_checksum(ip_header, udp_header, payload, payload_length);

    udp_header->checksum[0] = (checksum >> 8) & 0xFF;
    udp_header->checksum[1] = checksum & 0xFF;

    return true;
}
