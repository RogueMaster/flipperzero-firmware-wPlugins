#ifndef ICMP_H
#define ICMP_H

#include <furi.h>
#include <furi_hal.h>

#define ICMP_HEADER_LEN            8
#define ICMP_TYPE_ECHO_REPLY       0
#define ICMP_TYPE_ECHO_REQUEST     8
#define ICMP_TYPE_DEST_UNREACHABLE 3
#define ICMP_TYPE_TIME_EXCEEDED    11
#define ICMP_TYPE_REDIRECT         5

#define ICMP_CODE_PORT_UNREACHABLE 3

typedef struct {
    uint8_t type;
    uint8_t code;
    uint8_t checksum[2];
    uint8_t identifier[2];
    uint8_t sequence[2];
} icmp_header_t;

/**
 * @brief Populates an ICMP header in a given buffer.
 *
 * This function constructs an ICMP header within the provided buffer,
 * setting the message type, code, identifier, and sequence number. It then
 * calculates and sets the correct checksum for the entire ICMP message,
 * including the provided data.
 *
 * @param buffer A pointer to the buffer where the ICMP header will be written.
 * @param type The 8-bit ICMP message type (e.g., 8 for Echo Request).
 * @param code The 8-bit ICMP message code.
 * @param identifier The 16-bit identifier for the message.
 * @param sequence The 16-bit sequence number.
 * @param data A pointer to the data payload of the ICMP message.
 * @param data_length The length of the data payload in bytes.
 * @return `true` if the header was set successfully, `false` otherwise.
 */
bool icmp_set_header(
    uint8_t* buffer,
    uint8_t type,
    uint8_t code,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* data,
    uint16_t data_length);

/**
 * @brief Parses a network packet and extracts its ICMP header.
 *
 * This function reads the relevant bytes from a network packet buffer
 * (assuming it points to the start of the ICMP message) and populates an
 * `icmp_header_t` structure with the corresponding header information.
 *
 * @param buffer A pointer to the buffer containing the ICMP message.
 * @return An `icmp_header_t` structure containing the parsed ICMP header information.
 */
icmp_header_t icmp_get_header(uint8_t* buffer);

/**
 * @brief Calculates the ICMP header and data checksum.
 *
 * his function computes the 16-bit one's complement checksum for the
 * ICMP message. The calculation covers the entire ICMP message, starting with the
 * header and including the data payload.
 *
 * @param header A pointer to the `icmp_header_t` structure.
 * @param data A pointer to the data payload of the ICMP message.
 * @param data_length The length of the data payload in bytes.
 * @return The calculated 16-bit checksum value.
 */
uint16_t icmp_calculate_checksum(icmp_header_t* header, uint8_t* data, uint16_t data_length);

/**
 * @brief Checks if a network packet is an ICMP packet.
 *
 * This function typically inspects the Protocol field of the IPv4 header
 * to determine if the payload is an ICMP packet. The protocol number for ICMP is `1`.
 *
 * @param buffer A pointer to the buffer containing the start of the IPv4 packet.
 * @return `true` if the packet's payload is ICMP, `false` otherwise.
 */
bool is_icmp(uint8_t* buffer);

#endif // ICMP_H
