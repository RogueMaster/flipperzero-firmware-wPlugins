#include "icmp.h"
#include "ipv4.h"
#include "ethernet_protocol.h"

uint16_t icmp_calculate_checksum(icmp_header_t* header, uint8_t* data, uint16_t data_length) {
    uint32_t sum = 0;
    uint16_t total_length = ICMP_HEADER_LEN + data_length;

    // Create temporary buffer for checksum calculation
    uint8_t temp_buffer[total_length];

    // Copy header to temp buffer (with checksum set to 0)
    memcpy(temp_buffer, header, ICMP_HEADER_LEN);
    temp_buffer[2] = 0; // Clear checksum field
    temp_buffer[3] = 0;

    // Copy data to temp buffer
    if(data && data_length > 0) {
        memcpy(temp_buffer + ICMP_HEADER_LEN, data, data_length);
    }

    // Calculate checksum over header + data
    for(uint16_t i = 0; i < total_length - 1; i += 2) {
        sum += (temp_buffer[i] << 8) | temp_buffer[i + 1];
    }

    // Handle last byte if length is odd
    if(total_length & 1) {
        sum += temp_buffer[total_length - 1] << 8;
    }

    // Fold 32-bit sum into 16 bits
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum;
}

bool icmp_set_header(
    uint8_t* buffer,
    uint8_t type,
    uint8_t code,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* data,
    uint16_t data_length) {
    if(!buffer || !data) {
        return false;
    }

    icmp_header_t* header = (icmp_header_t*)buffer;

    // Initialize header
    memset(header, 0, sizeof(*header));

    // Set type and code
    header->type = type;
    header->code = code;

    // Set identifier (big endian)
    header->identifier[0] = (identifier >> 8) & 0xff;
    header->identifier[1] = identifier & 0xff;

    // Set sequence number (big endian)
    header->sequence[0] = (sequence >> 8) & 0xff;
    header->sequence[1] = sequence & 0xff;

    // Calculate and set checksum
    uint16_t checksum = icmp_calculate_checksum(header, data, data_length);
    header->checksum[0] = (checksum >> 8) & 0xff;
    header->checksum[1] = checksum & 0xff;

    return true;
}

icmp_header_t icmp_get_header(uint8_t* buffer) {
    icmp_header_t header = {0};

    if(!buffer) {
        return header;
    }

    // ICMP header comes after Ethernet + IPv4 headers
    uint8_t* icmp_start = buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN;

    memcpy((uint8_t*)&header, icmp_start, ICMP_HEADER_LEN);

    return header;
}

bool is_icmp(uint8_t* buffer) {
    if(!buffer) {
        return false;
    }

    // First check if it's IPv4
    if(!is_ipv4(buffer)) {
        return false;
    }

    // Get IPv4 header and check protocol field
    ipv4_header_t ip_header = ipv4_get_header(buffer);

    // ICMP protocol number is 1
    if(ip_header.protocol != 1) {
        return false;
    }

    return true;
}
