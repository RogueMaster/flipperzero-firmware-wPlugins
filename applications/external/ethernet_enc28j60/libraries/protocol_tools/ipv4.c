#include "ipv4.h"
#include "ethernet_protocol.h"

#include "app_user.h"

uint16_t calculate_checksum(uint8_t* data, uint16_t len) {
    uint32_t sum = 0;

    // Handle pairs of bytes
    for(uint16_t i = 0; i < len - 1; i += 2) {
        sum += (data[i] << 8) | data[i + 1];
    }

    // Handle last byte if length is odd
    if(len & 1) {
        sum += data[len - 1] << 8;
    }

    // Fold 32-bit sum into 16 bits
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

uint16_t calculate_checksum_ipv4(uint16_t* data, uint16_t len) {
    uint32_t sum = 0;
    uint16_t byte_act;

    for(int i = 0; i < len; i++) {
        byte_act = (uint16_t)((((uint8_t*)&data[i])[0] << 8) + ((uint8_t*)&data[i])[1]);
        sum += byte_act;
        if(sum & 0x10000) {
            sum = (sum & 0xFFFF) + 1;
        }
    }

    return ~sum & 0xFFFF;
}

bool set_ipv4_header(
    uint8_t* buffer,
    uint8_t protocol,
    uint16_t data_length,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t ip_id,
    uint16_t ip_flags_offset,
    uint8_t ttl) {
    // If any of the arrays are NULL, will return false
    if(!buffer || !src_ip || !dst_ip) {
        return false;
    }

    ipv4_header_t* header = (ipv4_header_t*)buffer;

    // Inicialize the header
    memset(header, 0, sizeof(*header));

    // Set version and length header
    header->version_ihl = 0x45;

    // Service = 0
    header->type_of_service = 0;

    // Total length
    uint16_t total_length = sizeof(ipv4_header_t) + data_length;
    header->total_length[0] = total_length >> 8;
    header->total_length[1] = (uint8_t)total_length;

    // Set Identification
    uint_to_bytes(&ip_id, header->identification, sizeof(uint16_t));

    // Set flags
    uint_to_bytes(&ip_flags_offset, header->flags_offset, sizeof(uint16_t));

    // Set time to live
    header->ttl = ttl; // 64;

    // Set protocol
    header->protocol = protocol;

    // Set IP
    memcpy(header->dest_ip, dst_ip, 4);
    memcpy(header->source_ip, src_ip, 4);

    // Set checksum
    uint16_t checksum = calculate_checksum_ipv4((uint16_t*)header, sizeof(ipv4_header_t) / 2);

    header->checksum[0] = (checksum >> 8) & 0xff;
    header->checksum[1] = checksum & 0xff;
    return true;
}

ipv4_header_t ipv4_get_header(uint8_t* buffer) {
    ipv4_header_t ip_header = {0};

    memcpy((uint8_t*)&ip_header, buffer + ETHERNET_HEADER_LEN, IP_HEADER_LEN);

    return ip_header;
}

bool is_ipv4(uint8_t* buffer) {
    ethernet_header_t header = ethernet_get_header(buffer);

    uint16_t type = header.type[0] << 8 | header.type[1];

    if(type != 0x0800) return false;

    return true;
}
