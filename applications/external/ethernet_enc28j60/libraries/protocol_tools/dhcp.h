#ifndef _DHCP_H_
#define _DHCP_H_

#include <furi.h>
#include <furi_hal.h>

// Parameters of type of message
#define DHCP_DISCOVER   1
#define DHCP_OFFER      2
#define DHCP_REQUEST    3
#define DHCP_ACKNOLEDGE 5

// Parameters
#define DHCP_OP_SUBNET_MASK              1
#define DHCP_OP_TIME_OFFSET              2
#define DHCP_OP_ROUTER                   3
#define DHCP_OP_TIME_SERVER              4
#define DHCP_OP_NAME_SERVER              5
#define DHCP_OP_DOMAIN_NAME_SERVER       6
#define DHCP_OP_LOG_SERVER               7
#define DHCP_OP_COOKIE_SERVER            8
#define DHCP_OP_LPR_SERVER               9
#define DHCP_OP_IMPRESS_SERVER           10
#define DHCP_OP_RESOURCE_LOCATION_SERVER 11
#define DHCP_OP_HOST_NAME                12
#define DHCP_OP_BOOT_FILE_SIZE           13
#define DHCP_OP_MERIT_DUMP_FILE          14
#define DHCP_OP_DOMAIN_NAME              15
#define DHCP_OP_SWAP_SERVER              16
#define DHCP_OP_ROOT_PATH                17
#define DHCP_OP_EXTENSIONS_PATH          18

// IP Parameters
#define DHCP_OP_INTERFACE_MTU               26
#define DHCP_OP_ALL_SUBNETS_ARE_LOCAL       27
#define DHCP_OP_BROADCAST_ADDRESS           28
#define DHCP_OP_PERFORM_MASK_DISCOVERY      29
#define DHCP_OP_MASK_SUPPLIER               30
#define DHCP_OP_PERFOM_ROUTER_DISCOVERY     31
#define DHCP_OP_ROUTER_SOLICITATION_ADDRESS 32
#define DHCP_OP_STATIC_ROUTE                33

// extensions DHCP
#define DHCP_OP_REQUESTED_IP           50
#define DHCP_OP_IP_ADDRESS_LEASE_TIME  51
#define DHCP_OP_OPTION_OVERLOAD        52
#define DHCP_OP_DHCP_MESSAGE_TYPE      53
#define DHCP_OP_SERVER_IDENTIFIER      54
#define DHCP_OP_PARAMETER_REQUEST_LIST 55
#define DHCP_OP_MESSAGE                56
#define DHCP_OP_MAXIMUM_MESSAGE_SIZE   57
#define DHCP_OP_CLIENT_IDENTIFIER      61

// End of message
#define DHCP_END 0xff

typedef struct {
    uint8_t operation;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint8_t xid[4];
    uint8_t secs[2];
    uint8_t flags[2];
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16];
    uint8_t sname[64];
    uint8_t file[128];
    uint8_t magic_cookie[4];
    uint8_t dhcp_options[1000];
} dhcp_message_t;

/**
 * @brief Creates a DHCP Discover message.
 *
 * This function constructs a DHCP Discover message, which is the
 * first step in the DHCP client-server interaction to find a DHCP server on the network.
 * It populates the necessary fields, including the client's MAC address, a unique
 * transaction ID (XID), and an optional host name.
 *
 * @param MAC_ADDRESS A pointer to the 6-byte MAC address of the client.
 * @param xid A unique 32-bit transaction ID.
 * @param host_name A pointer to a string containing the client's host name.
 * @param len A pointer to a `uint16_t` that will be updated with the total length of the DHCP message.
 * @return A `dhcp_message_t` structure representing the created Discover message.
 */
dhcp_message_t
    dhcp_message_discover(uint8_t* MAC_ADDRESS, uint32_t xid, uint8_t* host_name, uint16_t* len);

/**
 * @brief Creates a DHCP Request message.
 *
 * This function constructs a DHCP Request message, used by a client
 * to request a specific IP address from a DHCP server. It includes the client's
 * MAC address, a transaction ID, the requested IP address (`ip_client`), and the
 * IP address of the server (`ip_server`) that sent the initial offer.
 *
 * @param MAC_ADDRESS A pointer to the 6-byte MAC address of the client.
 * @param xid A unique 32-bit transaction ID.
 * @param ip_client A pointer to the 4-byte IP address being requested by the client.
 * @param ip_server A pointer to the 4-byte IP address of the DHCP server.
 * @param host_name A pointer to a string containing the client's host name.
 * @param len A pointer to a `uint16_t` that will be updated with the total length of the DHCP message.
 * @return A `dhcp_message_t` structure representing the created Request message.
 */
dhcp_message_t dhcp_message_request(
    uint8_t* MAC_ADDRESS,
    uint32_t xid,
    uint8_t* ip_client,
    uint8_t* ip_server,
    uint8_t* host_name,
    uint16_t* len);

/**
 * @brief Deconstructs a raw buffer into a DHCP message structure.
 *
 * @details This function parses a raw network packet buffer and extracts all the
 * fields of a DHCP message, populating a `dhcp_message_t` structure. This is
 * typically used by a DHCP server or client to interpret incoming DHCP packets.
 *
 * @param buffer A pointer to the buffer containing the DHCP message.
 * @return A `dhcp_message_t` structure containing the parsed DHCP message data.
 */
dhcp_message_t dhcp_deconstruct_dhcp_message(uint8_t* buffer);

/**
 * @brief Checks if a DHCP message is a Discover message.
 *
 * This function examines the DHCP message type field (usually an option)
 * to determine if the message is a DHCP Discover.
 *
 * @param message The `dhcp_message_t` structure to check.
 * @return `true` if the message is a DHCP Discover, `false` otherwise.
 */
bool dhcp_is_discover(dhcp_message_t message);

/**
 * @brief Checks if a DHCP message is an Offer message.
 *
 * This function examines the DHCP message type field to determine if the
 * message is a DHCP Offer, which is sent by a server in response to a Discover.
 *
 * @param message The `dhcp_message_t` structure to check.
 * @return `true` if the message is a DHCP Offer, `false` otherwise.
 */
bool dhcp_is_offer(dhcp_message_t message);

/**
 * @brief Checks if a DHCP message is a Request message.
 *
 * This function examines the DHCP message type field to determine if the
 * message is a DHCP Request, which is sent by a client to request an IP lease.
 *
 * @param message The `dhcp_message_t` structure to check.
 * @return `true` if the message is a DHCP Request, `false` otherwise.
 */
bool dhcp_is_request(dhcp_message_t message);

/**
 * @brief Checks if a DHCP message is an Acknowledgment message.
 *
 * This function examines the DHCP message type field to determine if the
 * message is a DHCP Acknowledgment (ACK), sent by a server to confirm a lease.
 *
 * @param message The `dhcp_message_t` structure to check.
 * @return `true` if the message is a DHCP Acknowledgment, `false` otherwise.
 */
bool dhcp_is_acknoledge(dhcp_message_t message);

/**
 * @brief Checks if a network packet is a DHCP packet.
 *
 * This function typically inspects the UDP port numbers of the packet
 * (source port 68, destination port 67) to determine if it is a DHCP message.
 *
 * @param buffer A pointer to the buffer containing the start of the UDP packet.
 * @return `true` if the packet is a DHCP message, `false` otherwise.
 */
bool is_dhcp(uint8_t* buffer);

/**
 * @brief Retrieves the data for a specific DHCP option.
 *
 * This function searches through the options section of a DHCP message
 * for a specific option code and, if found, copies the option's data and its
 * length into the provided buffers.
 *
 * @param message The `dhcp_message_t` structure containing the DHCP message.
 * @param option The option code to search for.
 * @param data A pointer to a buffer where the option's data will be copied.
 * @param len_data A pointer to a `uint8_t` that will be updated with the length of the data.
 * @return `true` if the option was found and data was retrieved successfully, `false` otherwise.
 */
bool dhcp_get_option_data(dhcp_message_t message, uint8_t option, uint8_t* data, uint8_t* len_data);

#endif
