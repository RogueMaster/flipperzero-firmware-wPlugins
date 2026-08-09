#ifndef DHCP_SETUP_H_
#define DHCP_SETUP_H_

#include <furi.h>
#include <furi_hal.h>
#include "../libraries/chip/enc28j60.h"

/**
 * @brief Performs the DORA process with an included host name.
 *
 * Handles the full DHCP DORA cycle (Discover, Offer, Request, Acknowledge)
 * and includes a user-defined host name in the DHCP Discover and Request
 * messages, so the server can register the device with a specific name.
 *
 * @param ethernet A pointer to the ENC28J60 driver instance.
 * @param static_ip A pointer to a 4-byte buffer where the assigned IP address will be stored.
 * @param ip_router A pointer to a 4-byte buffer where the router's IP address will be stored.
 * @param host A string containing the host name to be included in the DHCP messages.
 * @param cancel Optional. Pointer to a flag the caller can flip true to
 *               abort the DORA loop early. NULL disables cancellation.
 *               F0.5f — added so GetIPScene's Back can cancel a 10 s
 *               wait instead of freezing the UI in furi_thread_join.
 * @return `true` if the DORA process was successful, `false` otherwise.
 */
bool flipper_process_dora_with_host_name(
    enc28j60_t* ethernet,
    uint8_t* static_ip,
    uint8_t* ip_router,
    uint8_t* mac_router,
    const char* host,
    volatile const bool* cancel);

/**
 * @brief Retrieves the MAC address of the DHCP server.
 *
 * This function is intended to get the MAC address of the DHCP server
 * from a global or instance variable that was set during a successful DORA process.
 *
 * @param MAC_SERVER A pointer to a 6-byte buffer where the server's MAC address will be copied.
 */
void get_mac_server(uint8_t* MAC_SERVER);

/**
 * @brief Retrieves the IP address of the default gateway (router).
 *
 * This function is intended to get the IP address of the gateway
 * from a global or instance variable that was set during a successful DORA process.
 *
 * @param ip_gateway A pointer to a 4-byte buffer where the gateway's IP address will be copied.
 */
void get_gateway_ip(uint8_t* ip_gateway);

void get_subnet_mask(uint8_t* mask);

void set_dhcp_discover_message_with_host_name(uint8_t* buffer, uint16_t* length, const char* host);

void set_mac_address(uint8_t* mac_address);

#endif
