#include "ping_module.h"

#include "../libraries/protocol_tools/icmp.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/ethernet_protocol.h"

#define PING_DATA_SIZE 64 // 64 is the minimum size of the ICMP message (8 bytes)
#define PACKET_SIZE                                         \
    (PING_DATA_SIZE + ETHERNET_HEADER_LEN + IP_HEADER_LEN + \
     ICMP_HEADER_LEN) // Real size of the packet

// Función para obtener timestamp (simulado para Flipper Zero)
uint32_t get_flipper_timestamp() {
    return furi_get_tick();
}

//  Function to compare IP
bool compare_ip(uint8_t* ip_a, uint8_t* ip_b) {
    for(uint8_t i = 0; i < 4; i++) {
        if(ip_a[i] != ip_b[i]) return false;
    }
    return true;
}

// Function to create a ping packet
uint16_t create_flipper_ping_packet(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* payload,
    uint16_t payload_len) {
    // Check if there is not a NULL pointer
    if(!buffer || !src_mac || !src_ip || !dst_ip || !payload) return 0;

    // Check the size of payload
    if(payload_len > 64) {
        return 0;
    }

    // Pingpacket position
    uint8_t position = 0;

    // Array to set the payload
    uint8_t ping_data[PING_DATA_SIZE] = {0};

    // Copy the payload
    memcpy(ping_data, payload, payload_len);

    // add position
    position += payload_len;

    // Add padding
    if(position < 64) {
        for(uint8_t i = position; i < 64; i++) {
            ping_data[i] = i - position;
        }
    }

    // set Ethernet header
    if(!set_ethernet_header(buffer, src_mac, dst_mac, 0x0800)) {
        return 0;
    }

    // Set IPv4 header
    if(!set_ipv4_header(
           buffer + ETHERNET_HEADER_LEN,
           1, // Protocolo ICMP
           ICMP_HEADER_LEN + PING_DATA_SIZE,
           src_ip,
           dst_ip,
           0,
           0x4000,
           WIN_TTL)) {
        return 0;
    }

    // Set the header ICMP
    if(!icmp_set_header(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           ICMP_TYPE_ECHO_REQUEST,
           0, // code
           identifier,
           sequence,
           ping_data,
           PING_DATA_SIZE)) {
        return 0;
    }

    // Set the payload
    memcpy(
        buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN, ping_data, PING_DATA_SIZE);

    return PACKET_SIZE;
}

// Create a ping packet reply
uint16_t create_ping_packet_reply(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* payload,
    uint16_t payload_len) {
    // set Ethernet header
    if(!set_ethernet_header(buffer, src_mac, dst_mac, 0x0800)) {
        return 0;
    }

    // Set IPv4 header
    if(!set_ipv4_header(
           buffer + ETHERNET_HEADER_LEN,
           1, // Protocolo ICMP
           ICMP_HEADER_LEN + payload_len,
           src_ip,
           dst_ip,
           0,
           0x4000,
           WIN_TTL)) {
        return 0;
    }

    // // Set the header ICMP
    if(!icmp_set_header(
           buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN,
           ICMP_TYPE_ECHO_REPLY,
           0, // code
           identifier,
           sequence,
           payload,
           payload_len)) {
        return 0;
    }

    // Set the payload
    memcpy(buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN, payload, payload_len);

    return payload_len + ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN;
}

// Function to get the ping packet
bool ping_packet_replied(uint8_t* packet, uint8_t* ip_ping) {
    if(!packet || !ip_ping) return false;

    if(!is_icmp(packet)) return false;

    icmp_header_t icmp_header = icmp_get_header(packet);

    if(icmp_header.type != ICMP_TYPE_ECHO_REPLY) return false;

    ipv4_header_t ipv4_header = ipv4_get_header(packet);

    uint8_t* ip_des = ipv4_header.source_ip;

    return compare_ip(ip_ping, ip_des);
}

// Function to get the ping packet request
bool ping_packet_requested(uint8_t* packet, uint8_t* own_ip) {
    if(!packet || !own_ip) return false;

    if(!is_icmp(packet)) return false;

    icmp_header_t icmp_header = icmp_get_header(packet);

    if(icmp_header.type != ICMP_TYPE_ECHO_REQUEST) return false;

    ipv4_header_t ipv4_header = ipv4_get_header(packet);

    uint8_t* ip_des = ipv4_header.dest_ip;

    return compare_ip(own_ip, ip_des);
}

// Function to reply a ping packet request to our ip
bool ping_reply_to_request(enc28j60_t* ethernet, uint8_t* packet, uint16_t size_of_packet) {
    if(!ethernet || !packet) return false;
    if(!ping_packet_requested(packet, ethernet->ip_address)) return false;

    // Get the ethernet header to solve the mac address
    ethernet_header_t ethernet_header = ethernet_get_header(packet);

    // Get ipv4 header to solve the IP address
    ipv4_header_t ip_header = ipv4_get_header(packet);

    // Get icmp header
    icmp_header_t icmp_header = icmp_get_header(packet);

    // Get the mac address for the destination
    uint8_t* mac_destination = ethernet_header.mac_source;

    // Get the IP address for the destination
    uint8_t* ip_to_send = ip_header.source_ip;

    // Get the ICMP identifier
    uint16_t identifier = icmp_header.identifier[0] << 8 | icmp_header.identifier[1];

    // Get the sequence
    uint16_t sequence = icmp_header.sequence[0] << 8 | icmp_header.sequence[1];

    // Get data from packet
    uint16_t data_size = size_of_packet - (ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN);
    uint8_t* data_extra = (uint8_t*)calloc(data_size, sizeof(uint8_t));
    memcpy(data_extra, packet + ETHERNET_HEADER_LEN + IP_HEADER_LEN + ICMP_HEADER_LEN, data_size);

    uint8_t packet_reply[MAX_FRAMELEN] = {0};

    uint16_t len = create_ping_packet_reply(
        packet_reply,
        ethernet->mac_address,
        mac_destination,
        ethernet->ip_address,
        ip_to_send,
        identifier,
        sequence,
        data_extra,
        data_size);

    send_packet(ethernet, packet_reply, len);

    free(data_extra);

    return true;
}
