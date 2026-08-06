#include "analysis_module.h"

#include "../libraries/protocol_tools/ethernet_protocol.h"
#include "../libraries/protocol_tools/ipv4.h"
#include "../libraries/protocol_tools/arp.h"
#include "../libraries/protocol_tools/udp.h"
#include "../libraries/protocol_tools/tcp.h"
#include "../libraries/protocol_tools/dhcp.h"

enum {
    ARP,
    IPV4,
    UDP,
    TCP,
    DHCP,
    UNKNOWN
} protocol_enum;

/**
 * Function to convert bytes to readable format
 */
void bytes_to_uint16(uint8_t* bytes, uint16_t* result) {
    *result = (bytes[0] << 8) | bytes[1];
}

void bytes_to_uint32(uint8_t* bytes, uint32_t* result) {
    *result = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

/**
 * This function extracts all packet information from Ethernet frames
 *
 * It will set:
 *      - MAC source and destination
 *      - IP source and destination (if available)
 *      - Protocol type (ARP, TCP, UDP, etc.)
 *      - Port information (for TCP/UDP)
 *      - Additional protocol-specific information
 *
 * Returns the size of the payload
 */
uint16_t get_packet_information(
    uint8_t* packet,
    uint16_t len,
    uint8_t* mac_source,
    uint8_t* mac_destiny,
    uint8_t* ip_source,
    uint8_t* ip_destininy,
    uint16_t* packet_type,
    uint8_t* type_message,
    uint16_t* port_source,
    uint16_t* port_destiny,
    uint8_t* payload) {
    if(!packet || !mac_destiny || !mac_source || !ip_source || !ip_destininy || !payload) {
        *packet_type = UNKNOWN;
        if(port_destiny) *port_destiny = 0;
        if(port_source) *port_source = 0;
        return 0;
    }

    // Initialize all variables
    memset(mac_source, 0, 6);
    memset(mac_destiny, 0, 6);
    memset(ip_source, 0, 4);
    memset(ip_destininy, 0, 4);
    *packet_type = UNKNOWN;
    if(type_message) *type_message = 0;
    if(port_source) *port_source = 0;
    if(port_destiny) *port_destiny = 0;

    // Get Ethernet header
    ethernet_header_t ethernet_header = ethernet_get_header(packet);
    memcpy(mac_source, ethernet_header.mac_source, 6);
    memcpy(mac_destiny, ethernet_header.mac_destination, 6);

    uint16_t payload_size = 0;

    // Check protocol type
    if(is_arp(packet)) {
        *packet_type = ARP;
        arp_header_t arp_header = arp_get_header(packet);

        // For ARP, copy IP addresses
        memcpy(ip_source, arp_header.ip_source, 4);
        memcpy(ip_destininy, arp_header.ip_destiny, 4);

        // Store operation code in type_message
        if(type_message) {
            *type_message = (arp_header.operation_code[0] << 8) | arp_header.operation_code[1];
        }

        // ARP doesn't have ports or data payload
        return 0;
    } else if(is_ipv4(packet)) {
        ipv4_header_t ip_header = ipv4_get_header(packet);

        // Copy IP addresses
        memcpy(ip_source, ip_header.source_ip, 4);
        memcpy(ip_destininy, ip_header.dest_ip, 4);

        // Calculate IPv4 payload size
        uint16_t total_length = (ip_header.total_length[0] << 8) | ip_header.total_length[1];
        uint16_t ip_header_length = (ip_header.version_ihl & 0x0F) * 4;

        // Check if it's UDP
        if(is_udp(packet)) {
            *packet_type = UDP;
            udp_header_t udp_header = udp_get_header(packet);

            // Get ports
            if(port_source) {
                *port_source = (udp_header.source_port[0] << 8) | udp_header.source_port[1];
            }
            if(port_destiny) {
                *port_destiny = (udp_header.dest_port[0] << 8) | udp_header.dest_port[1];
            }

            // Check if it's DHCP
            if(is_dhcp(packet)) {
                *packet_type = DHCP;
                dhcp_message_t dhcp_msg = dhcp_deconstruct_dhcp_message(packet);

                // Store DHCP message type
                if(type_message) {
                    if(dhcp_is_discover(dhcp_msg))
                        *type_message = DHCP_DISCOVER;
                    else if(dhcp_is_offer(dhcp_msg))
                        *type_message = DHCP_OFFER;
                    else if(dhcp_is_request(dhcp_msg))
                        *type_message = DHCP_REQUEST;
                    else if(dhcp_is_acknoledge(dhcp_msg))
                        *type_message = DHCP_ACKNOLEDGE;
                }
            }

            // Calculate UDP payload
            uint16_t udp_length = (udp_header.length[0] << 8) | udp_header.length[1];
            payload_size = udp_length - UDP_HEADER_LEN;

            // Copy payload
            if(payload_size > 0 &&
               payload_size <= (len - ETHERNET_HEADER_LEN - ip_header_length - UDP_HEADER_LEN)) {
                memcpy(
                    payload,
                    packet + ETHERNET_HEADER_LEN + ip_header_length + UDP_HEADER_LEN,
                    payload_size);
            }
        }
        // Check if it's TCP
        else if(is_tcp(packet)) {
            *packet_type = TCP;
            tcp_header_t tcp_header = tcp_get_header(packet);

            // Get ports
            if(port_source) {
                *port_source = (tcp_header.source_port[0] << 8) | tcp_header.source_port[1];
            }
            if(port_destiny) {
                *port_destiny = (tcp_header.dest_port[0] << 8) | tcp_header.dest_port[1];
            }

            // Store TCP flags in type_message
            /*if(type_message) {
                *type_message =
                    ((tcp_header.data_offset_flags[0] << 8) + tcp_header.data_offset_flags[1]) &
                    0x1FF;
            }*/

            // Calculate TCP header length and payload
            /*uint8_t tcp_header_length = (tcp_header.data_offset >> 4) * 4;
            payload_size = total_length - ip_header_length - tcp_header_length;*/

            // Copy payload
            /*if(payload_size > 0 && payload_size <= (len - ETHERNET_HEADER_LEN - ip_header_length -
                                                    tcp_header_length)) {
                memcpy(
                    payload,
                    packet + ETHERNET_HEADER_LEN + ip_header_length + tcp_header_length,
                    payload_size);
            }*/
        } else {
            // Other IPv4 protocols
            *packet_type = IPV4;
            if(type_message) {
                *type_message = ip_header.protocol;
            }
            payload_size = total_length - ip_header_length;
            if(payload_size > 0 &&
               payload_size <= (len - ETHERNET_HEADER_LEN - ip_header_length)) {
                memcpy(payload, packet + ETHERNET_HEADER_LEN + ip_header_length, payload_size);
            }
        }
    } else {
        // Unknown protocol
        *packet_type = UNKNOWN;
        // Copy raw payload after Ethernet header
        payload_size = len - ETHERNET_HEADER_LEN;
        if(payload_size > 0) {
            memcpy(payload, packet + ETHERNET_HEADER_LEN, payload_size);
        }
    }

    return payload_size;
}

// Get the protocol name
const char* get_protocol_name(uint16_t packet_type) {
    switch(packet_type) {
    case ARP:
        return "ARP";
    case IPV4:
        return "IPv4";
    case UDP:
        return "UDP";
    case TCP:
        return "TCP";
    case DHCP:
        return "DHCP";
    default:
        return "UNKNOWN";
    }
}

// Format a Mac Address
void format_mac_address(uint8_t* mac, char* str, size_t str_size) {
    snprintf(
        str,
        str_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

// Format an Ip Address
void format_ip_address(uint8_t* ip, char* str, size_t str_size) {
    snprintf(str, str_size, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

// Print string format
void set_string_info(
    FuriString* string,
    uint8_t* mac_source,
    uint8_t* mac_destiny,
    uint8_t* ip_source,
    uint8_t* ip_destininy,
    uint16_t packet_type,
    uint8_t type_message,
    uint16_t port_source,
    uint16_t port_destiny,
    uint16_t payload_size) {
    char mac_src_str[18];
    char mac_dst_str[18];
    char ip_src_str[16];
    char ip_dst_str[16];

    format_mac_address(mac_source, mac_src_str, sizeof(mac_src_str));
    format_mac_address(mac_destiny, mac_dst_str, sizeof(mac_dst_str));

    furi_string_cat_printf(string, "--- Packet Information ---\n");
    furi_string_cat_printf(string, "  EtherType: %s\n", get_protocol_name(packet_type));

    furi_string_cat_printf(string, "Ethernet Header:\n");
    furi_string_cat_printf(string, "  Source MAC: %s\n", mac_src_str);
    furi_string_cat_printf(string, "  Destination MAC: %s\n", mac_dst_str);

    if(packet_type == IPV4 || packet_type == UDP || packet_type == TCP || packet_type == DHCP) {
        format_ip_address(ip_source, ip_src_str, sizeof(ip_src_str));
        format_ip_address(ip_destininy, ip_dst_str, sizeof(ip_dst_str));
        furi_string_cat_printf(string, "IP Header:\n");
        furi_string_cat_printf(string, "  Source IP: %s\n", ip_src_str);
        furi_string_cat_printf(string, "  Destination IP: %s\n", ip_dst_str);
        furi_string_cat_printf(string, "  Protocol: %s\n", get_protocol_name(packet_type));
    }

    if(packet_type == ARP) {
        format_ip_address(ip_source, ip_src_str, sizeof(ip_src_str));
        format_ip_address(ip_destininy, ip_dst_str, sizeof(ip_dst_str));
        furi_string_cat_printf(string, "ARP Information:\n");
        furi_string_cat_printf(string, "  Source IP: %s\n", ip_src_str);
        furi_string_cat_printf(string, "  Destination IP: %s\n", ip_dst_str);
        furi_string_cat_printf(string, "  Message Type: ");
        if(type_message == 0x1) {
            furi_string_cat_printf(string, "Request\n");
        } else if(type_message == 0x2) {
            furi_string_cat_printf(string, "Reply\n");
        } else {
            furi_string_cat_printf(string, "Unknown (%d)\n", type_message);
        }
    } else if(packet_type == UDP || packet_type == TCP) {
        furi_string_cat_printf(
            string, "Transport Layer Information (%s):\n", get_protocol_name(packet_type));
        furi_string_cat_printf(string, "  Source Port: %u\n", port_source);
        furi_string_cat_printf(string, "  Destination Port: %u\n", port_destiny);
    }

    if(packet_type == DHCP) {
        furi_string_cat_printf(string, "DHCP Information:\n");
        furi_string_cat_printf(string, "  Message Type: ");
        switch(type_message) {
        case DHCP_DISCOVER:
            furi_string_cat_printf(string, "Discover\n");
            break;
        case DHCP_OFFER:
            furi_string_cat_printf(string, "Offer\n");
            break;
        case DHCP_REQUEST:
            furi_string_cat_printf(string, "Request\n");
            break;
        default:
            furi_string_cat_printf(string, "Unknown (%d)\n", type_message);
            break;
        }
    }

    furi_string_cat_printf(string, "Payload Size: %u bytes\n", payload_size);
    furi_string_cat_printf(string, "--------------------\n");
}

void print_packet_info(FuriString* text, uint8_t* packet, uint16_t packet_size) {
    uint8_t payload[1500] = {0};
    uint8_t mac_source[6] = {0};
    uint8_t mac_destiny[6] = {0};
    uint8_t ip_source[4] = {0};
    uint8_t ip_destiny[4] = {0};

    uint16_t packet_type = 0;
    uint16_t port_source = 0;
    uint16_t port_destiny = 0;

    uint8_t type_message = 0;

    uint16_t payload_size = 0;

    payload_size = get_packet_information(
        packet,
        packet_size,
        mac_source,
        mac_destiny,
        ip_source,
        ip_destiny,
        &packet_type,
        &type_message,
        &port_source,
        &port_destiny,
        payload);

    set_string_info(
        text,
        mac_source,
        mac_destiny,
        ip_source,
        ip_destiny,
        packet_type,
        type_message,
        port_source,
        port_destiny,
        payload_size);
}
