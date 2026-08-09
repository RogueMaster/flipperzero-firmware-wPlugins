#ifndef _IPV4_H_
#define _IPV4_H_

#include <furi.h>
#include <furi_hal.h>

#define IP_HEADER_LEN 20

typedef struct {
    uint8_t version_ihl; // 4 bits for the versión + 4 bits of header length (IHL)
    uint8_t type_of_service; // type of service
    uint8_t total_length[2]; // Total length (2 bytes)
    uint8_t identification[2]; // Identification (2 bytes)
    uint8_t flags_offset[2]; // 3 bits for flags + 13 bits for fragment offset(2 bytes)
    uint8_t ttl; // time to live (TTL)
    uint8_t protocol; // Protocol (TCP, UDP, etc.)
    uint8_t checksum[2]; // Checksum (2 bytes)
    uint8_t source_ip[4]; // origin ip(4 bytes)
    uint8_t dest_ip[4]; // destination ip(4 bytes)
} ipv4_header_t;

/**
 * @brief Calculates the IPv4 header checksum.
 *
 * This function computes the 16-bit one's complement checksum of a
 * provided data block. It is a crucial step for ensuring the integrity of
 * the IPv4 header before transmission.
 *
 * @param data A pointer to the data block (typically the IPv4 header) to checksum.
 * @param len The length of the data block in bytes.
 * @return The calculated 16-bit checksum value.
 */
uint16_t calculate_checksum(uint8_t* data, uint16_t len);
uint16_t calculate_checksum_ipv4(uint16_t* data, uint16_t len);

/**
 * @brief Populates an IPv4 header in a given buffer.
 *
 * This function takes a raw buffer and fills it with a complete IPv4 header.
 * It sets the version, IHL, protocol, total length, and source and destination
 * IP addresses. It also calculates and sets the correct checksum for the header.
 *
 * @param buffer A pointer to the buffer where the IPv4 header will be written.
 * @param protocol The 8-bit protocol number (e.g., `IP_PROTOCOL_UDP`).
 * @param data_length The length of the payload data, excluding the IPv4 header itself.
 * @param src_ip A pointer to the 4-byte source IP address.
 * @param dst_ip A pointer to the 4-byte destination IP address.
 * @return `true` if the header was set successfully, `false` otherwise.
 */
bool set_ipv4_header(
    uint8_t* buffer,
    uint8_t protocol,
    uint16_t data_length,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t ip_id,
    uint16_t ip_flags_offset,
    uint8_t ttl);

/**
 * @brief Parses a network packet and extracts the IPv4 header.
 *
 * This function reads the relevant bytes from a network packet buffer
 * and populates an `ipv4_header_t` structure with the corresponding
 * IPv4 header information.
 *
 * @param buffer A pointer to the buffer containing the IPv4 packet.
 * @return An `ipv4_header_t` structure containing the parsed header information.
 */
ipv4_header_t ipv4_get_header(uint8_t* buffer);

/**
 * @brief Checks if a network frame is an IPv4 packet.
 *
 * This function typically inspects the EtherType field of the Ethernet
 * header to determine if the payload is an IPv4 packet. The EtherType for
 * IPv4 is `0x0800`.
 *
 * @param buffer A pointer to the buffer containing the start of the Ethernet frame.
 * @return `true` if the frame's payload is IPv4, `false` otherwise.
 */
bool is_ipv4(uint8_t* buffer);

#endif
