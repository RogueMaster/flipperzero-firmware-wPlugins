#ifndef PING_MODULE_H_
#define PING_MODULE_H_

/**
 * Note!!
 * This module still in development, the ping request is not working well
 * and need to do some fixes. Then we will fix this problem.
 */

#include <furi.h>
#include <furi_hal.h>

#include "../libraries/chip/enc28j60.h"

/**
 * @brief Creates a complete ICMP Echo Request packet.
 *
 * This function assembles a full network packet with Ethernet, IPv4,
 * and ICMP headers, followed by a data payload. It is specifically designed
 * to create a ping request message for a target device. The function automatically
 * handles the calculation of the IPv4 and ICMP checksums.
 *
 * @param buffer A pointer to the buffer where the complete packet will be assembled.
 * @param src_mac A pointer to the 6-byte source MAC address.
 * @param dst_mac A pointer to the 6-byte destination MAC address.
 * @param src_ip A pointer to the 4-byte source IP address.
 * @param dst_ip A pointer to the 4-byte destination IP address.
 * @param identifier The 16-bit identifier for the ICMP message.
 * @param sequence The 16-bit sequence number for the ICMP message.
 * @param payload A pointer to the data payload for the ICMP message.
 * @param payload_len The length of the payload in bytes.
 * @return The total length of the created packet in bytes.
 */
uint16_t create_flipper_ping_packet(
    uint8_t* buffer,
    uint8_t* src_mac,
    uint8_t* dst_mac,
    uint8_t* src_ip,
    uint8_t* dst_ip,
    uint16_t identifier,
    uint16_t sequence,
    uint8_t* payload,
    uint16_t payload_len);

/**
 * @brief Checks if a network packet is an ICMP Echo Reply to a specific ping.
 *
 * This function analyzes a received packet to determine if it is an
 * ICMP Echo Reply (`type=0`) and if the source IP address matches the IP address
 * of the device that was pinged.
 *
 * @param packet A pointer to the buffer containing the received packet.
 * @param ip_ping A pointer to the 4-byte IP address of the device that was pinged.
 * @return `true` if the packet is the expected ping reply, `false` otherwise.
 */
bool ping_packet_replied(uint8_t* packet, uint8_t* ip_ping);

/**
 * @brief Checks if a network packet is an ICMP Echo Request intended for this device.
 *
 * This function examines a received packet to determine if it is an
 * ICMP Echo Request (`type=8`) and if its destination IP address matches the
 * device's own IP address.
 *
 * @param packet A pointer to the buffer containing the received packet.
 * @param own_ip A pointer to the 4-byte IP address of this device.
 * @return `true` if the packet is a ping request for this device, `false` otherwise.
 */
bool ping_packet_requested(uint8_t* packet, uint8_t* own_ip);

/**
 * @brief Creates and sends an ICMP Echo Reply in response to a received request.
 *
 * This function takes a received ICMP Echo Request packet, swaps the
 * source and destination addresses, changes the ICMP type to Echo Reply (`type=0`),
 * recalculates the checksums, and transmits the resulting packet back to the sender.
 *
 * @param ethernet A pointer to the ENC28J60 driver instance.
 * @param packet A pointer to the buffer containing the received ICMP Echo Request packet.
 * @param size_of_packet The total length of the received packet.
 * @return `true` if the reply was successfully created and sent, `false` otherwise.
 */
bool ping_reply_to_request(enc28j60_t* ethernet, uint8_t* packet, uint16_t size_of_packet);

#endif
