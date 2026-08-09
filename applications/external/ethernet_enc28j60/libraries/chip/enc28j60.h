/*
 * Portions of this file are derived from the EtherCard library
 * (https://github.com/njh/EtherCard), originally authored by Guido Socher
 * and Jean-Claude Wippler, itself based on AVRlib by Pascal Stang.
 * EtherCard is licensed under GPL-2.0-or-later. See LICENSES/EtherCard.LICENSE.
 *
 * Modifications by Electronic Cats, 2024-2025: ported to Flipper Zero
 * (Furi HAL SPI), C-only, struct-based instance API.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _ENC28J60_LIBRARY_
#define _ENC28J60_LIBRARY_

#include <furi.h>
#include <furi_hal.h>

#include "Spi_lib.h"

#define MAX_FRAMELEN 1518

#define RXSTART_INIT 0x0000
#define RXSTOP_INIT  0x0BFF

#define TXSTART_INIT 0x0C00
#define TXSTOP_INIT  0x11FF

#define ETHERCARD_SEND_PIPELINING      0
#define ETHERCARD_RETRY_LATECOLLISIONS 0

#ifdef __cplusplus
extern "C" {
#endif

// Struct for the ENC28J60 Ethernet controller
typedef struct {
    FuriHalSpiBusHandle* spi; // SPI interface handle
    FuriMutex* mutex; // F0.5a — serializes chip register access (bank state)
    uint8_t mac_address[6]; // MAC address (6 bytes)
    uint8_t ip_address[4]; // IP address (4 bytes)
    uint8_t subnet_mask[4];
    uint8_t tx_buffer[MAX_FRAMELEN]; // Transmit buffer
    uint8_t rx_buffer[MAX_FRAMELEN]; // Receive buffer
} enc28j60_t;

/**
 * @brief Allocates and initializes a new ENC28J60 controller instance.
 *
 * This function allocates memory for an `enc28j60_t` structure and initializes
 * it with the provided MAC and IP addresses.
 *
 * @param mac_address Pointer to a 6-byte array representing the MAC address of the ENC28J60.
 * @param ip_address  Pointer to a 4-byte array representing the IP address assigned to the ENC28J60.
 *
 * @return Pointer to an initialized `enc28j60_t` structure, or `NULL` if allocation fails.
 */
enc28j60_t* enc28j60_alloc(uint8_t* mac_address, uint8_t* ip_address);

/**
 * @brief Frees the memory associated with an ENC28J60 instance.
 *
 * This function releases all dynamically allocated memory and resources
 * associated with the given ENC28J60 instance.
 *
 * @param instance Pointer to the `enc28j60_t` instance to be freed.
 */
void free_enc28j60(enc28j60_t* instance);

/**
 * @brief Performs a soft reset on the ENC28J60 controller.
 *
 * This function sends a system reset command to the ENC28J60,
 * returning it to its default state. The internal configuration and
 * registers may need to be reinitialized after the reset.
 *
 * @param instance Pointer to the `enc28j60_t` instance to reset.
 */
void enc28j60_soft_reset(enc28j60_t* instance);

/**
 * @brief Writes the MAC address to the ENC28J60's internal registers.
 *
 * This function sets the MAC address of the ENC28J60 using the value
 * stored in the `instance->mac_address` field. This should be called
 * after initialization or a soft reset to ensure the correct MAC is used.
 *
 * @param instance Pointer to the `enc28j60_t` instance whose MAC address will be set.
 */
void enc28j60_set_mac(enc28j60_t* instance);

/**
 * @brief Initializes and starts the ENC28J60 controller.
 *
 * This function performs the necessary configuration to bring the ENC28J60
 * into a ready state, including setting up buffers, registers, and MAC address.
 *
 * @param instance Pointer to the `enc28j60_t` instance to start.
 *
 * @return 0 on success, or a non-zero error code on failure.
 */
uint8_t enc28j60_start(enc28j60_t* instance);

/**
 * @brief Checks if the ENC28J60 has an active Ethernet link.
 *
 * This function reads the PHY status register to determine whether
 * the Ethernet cable is connected and the link is active.
 *
 * @param instance Pointer to the `enc28j60_t` instance to check.
 *
 * @return `true` if the link is up, `false` otherwise.
 */
bool is_link_up(enc28j60_t* instance);

/**
 * @brief Checks if the ENC28J60 is connected to a working network.
 *
 * This function verifies whether the ENC28J60 has a valid physical link
 * and is logically connected to the network. It may combine checks such as
 * link status and IP configuration, depending on implementation.
 *
 * @param instance Pointer to the `enc28j60_t` instance to check.
 *
 * @return `true` if the network connection is established, `false` otherwise.
 */
bool is_the_network_connected(enc28j60_t* instance);

/**
 * @brief Receives an Ethernet packet from the ENC28J60.
 *
 * This function attempts to read a received Ethernet frame from the ENC28J60's
 * internal buffer and copies its contents into the provided buffer.
 *
 * @param instance Pointer to the `enc28j60_t` instance.
 * @param buffer   Pointer to the destination buffer where the packet data will be stored.
 * @param size     Maximum number of bytes to read into the buffer.
 *
 * @return The number of bytes actually received and copied to the buffer,
 *         or 0 if no packet is available or an error occurred.
 */
uint16_t receive_packet(enc28j60_t* instance, uint8_t* buffer, uint16_t size);

/**
 * @brief Sends an Ethernet packet using the ENC28J60.
 *
 * This function copies the data from the provided buffer into the ENC28J60's
 * transmit buffer and initiates the transmission of the Ethernet frame.
 *
 * @param instance Pointer to the `enc28j60_t` instance.
 * @param buffer   Pointer to the buffer containing the packet data to send.
 * @param len      Length in bytes of the packet data to transmit.
 */
void send_packet(enc28j60_t* instance, uint8_t* buffer, uint16_t len);

/**
 * @brief Enables reception of broadcast Ethernet frames on the ENC28J60.
 *
 * This function configures the ENC28J60 to accept broadcast packets,
 * allowing it to receive frames sent to the broadcast MAC address (FF:FF:FF:FF:FF:FF).
 *
 * @param instance Pointer to the `enc28j60_t` instance to configure.
 */
void enable_broadcast(enc28j60_t* instance);

/**
 * @brief Disables reception of broadcast Ethernet frames on the ENC28J60.
 *
 * This function configures the ENC28J60 to ignore broadcast packets,
 * preventing it from receiving frames sent to the broadcast MAC address (FF:FF:FF:FF:FF:FF).
 *
 * @param instance Pointer to the `enc28j60_t` instance to configure.
 */
void disable_broadcast(enc28j60_t* instance);

/**
 * @brief Enables reception of multicast Ethernet frames on the ENC28J60.
 *
 * This function configures the ENC28J60 to accept multicast packets,
 * allowing it to receive frames sent to multicast MAC addresses.
 *
 * @param instance Pointer to the `enc28j60_t` instance to configure.
 */
void enable_multicast(enc28j60_t* instance);

/**
 * @brief Disables reception of multicast Ethernet frames on the ENC28J60.
 *
 * This function configures the ENC28J60 to ignore multicast packets,
 * preventing it from receiving frames sent to multicast MAC addresses.
 *
 * @param instance Pointer to the `enc28j60_t` instance to configure.
 */
void disable_multicast(enc28j60_t* instance);

/**
 * @brief Enables promiscuous mode on the ENC28J60 Ethernet controller.
 *
 * When enabled, the ENC28J60 will receive all Ethernet frames regardless of
 * their destination MAC address. This mode is typically used for network
 * debugging, monitoring, or packet sniffing.
 *
 * @param instance Pointer to the `enc28j60_t` instance to configure.
 */
void enable_promiscuous(enc28j60_t* instance);

/**
 * @brief Disables promiscuous mode on the ENC28J60 Ethernet controller.
 *
 * When disabled, the ENC28J60 filters incoming Ethernet frames and only
 * accepts those addressed to its MAC address or broadcast/multicast addresses,
 * depending on the configuration.
 *
 * @param instance Pointer to the `enc28j60_t` instance to configure.
 */
void disable_promiscuous(enc28j60_t* instance);

/**
 * @brief Generates a random, locally administered MAC address.
 *
 * This function fills the provided 6-byte array with a randomly generated
 * MAC address. The generated address will have the locally administered bit set
 * and the multicast bit cleared, ensuring it is a unicast address suitable for local use.
 *
 * @param mac Pointer to a 6-byte array where the generated MAC address will be stored.
 */
void generate_random_mac(uint8_t* mac);

/**
 * @brief Displays or processes a message stored in a buffer.
 *
 * This function handles the content of the provided buffer, which may contain
 * a message or data of specified length. The exact behavior depends on the implementation
 * (e.g., printing to console, logging, or sending to a display).
 *
 * @param buffer Pointer to the buffer containing the message data.
 * @param len    Length in bytes of the message data.
 */
void show_message(uint8_t* buffer, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
