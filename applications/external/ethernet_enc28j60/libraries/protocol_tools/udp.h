#ifndef _UDP_H_
#define _UDP_H_

#include <furi.h>
#include <furi_hal.h>

#define UDP_HEADER_LEN 8

typedef struct {
    uint8_t source_port[2]; // Source Port(2 bytes)
    uint8_t dest_port[2]; // Puerto de destino (2 bytes)
    uint8_t length[2]; // header length + data (2 bytes)
    uint8_t checksum[2]; // Checksum (2 bytes)
} udp_header_t;

/**
 * @brief Populates a UDP header in a given buffer.
 *
 * This function constructs a UDP header within the provided buffer,
 * setting the source port, destination port, and total length of the UDP segment
 * (header + payload). The checksum field is typically calculated and set separately.
 *
 * @param buffer A pointer to the buffer where the UDP header will be written.
 * @param source_port The 16-bit source port number.
 * @param destination_port The 16-bit destination port number.
 * @param length The 16-bit length of the UDP header and data in bytes.
 * @return `true` if the header was successfully set, `false` otherwise.
 */
bool set_udp_header(
    uint8_t* buffer,
    uint16_t source_port,
    uint16_t destination_port,
    uint16_t length);

/**
 * @brief Parses a network packet and extracts its UDP header.
 *
 * This function reads the relevant bytes from a network packet buffer
 * (assuming it points to the start of the UDP segment) and populates a
 * `udp_header_t` structure with the corresponding UDP header information.
 *
 * @param buffer A pointer to the buffer containing the UDP segment.
 * @return A `udp_header_t` structure containing the parsed UDP header information.
 */
udp_header_t udp_get_header(uint8_t* buffer);

/**
 * @brief Checks if a network packet is a UDP packet.
 *
 * This function typically inspects the Protocol field of the IPv4 header
 * to determine if the payload is a UDP packet. The protocol number for UDP is `17`.
 *
 * @param buffer A pointer to the buffer containing the start of the IPv4 packet.
 * @return `true` if the packet's payload is UDP, `false` otherwise.
 */
bool is_udp(uint8_t* buffer);

/**
 * @brief Creates a complete UDP packet (Ethernet + IPv4 + UDP + Payload).
 *
 * @details This comprehensive function assembles a full UDP packet by
 * sequentially building the Ethernet, IPv4, and UDP headers, and then
 * appending the provided payload. It also handles the calculation of
 * the IPv4 checksum. The UDP checksum is often optional and may need
 * to be calculated separately if required.
 *
 * @param buffer A pointer to the buffer where the complete packet will be assembled.
 * @param src_mac A pointer to the 6-byte source MAC address.
 * @param dst_mac A pointer to the 6-byte destination MAC address.
 * @param src_ip A pointer to the 4-byte source IP address.
 * @param dst_ip A pointer to the 4-byte destination IP address.
 * @param src_port The 16-bit source UDP port.
 * @param dst_port The 16-bit destination UDP port.
 * @param payload A pointer to the application layer data (payload).
 * @param payload_length The length of the payload in bytes.
 * @return `true` if the complete UDP packet was successfully created, `false` otherwise.
 */
bool create_udp_packet(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t src_port,
    uint16_t dst_port,
    uint8_t* payload,
    uint16_t payload_length);

#endif
