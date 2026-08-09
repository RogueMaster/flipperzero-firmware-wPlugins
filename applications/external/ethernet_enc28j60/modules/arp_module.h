#ifndef ARP_HELPER_H_
#define ARP_HELPER_H_

#include <furi.h>
#include <furi_hal.h>
#include "../libraries/chip/enc28j60.h"

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
} arp_list;

/**
 * @brief Creates a complete ARP reply message for an ARP spoofing attack on all devices.
 *
 * This function constructs a fake ARP reply packet. It sets the source MAC to the attacker's
 * MAC and the source IP to the router's IP, effectively telling all devices on the network that
 * the attacker is the router. The destination MAC is set to a broadcast address. This is used in
 * a "gateway-spoofing" or "ARP broadcast" attack.
 *
 * @param buffer A pointer to the buffer where the ARP message will be constructed.
 * @param MAC A pointer to the 6-byte MAC address of the attacker.
 * @param ip_for_router A pointer to the 4-byte IP address of the legitimate router.
 * @param len A pointer to a `uint16_t` that will be updated with the total length of the ARP message.
 */
void set_arp_message_for_attack_all(
    uint8_t* buffer,
    uint8_t* MAC,
    uint8_t* ip_for_router,
    uint16_t* len);

/**
 * @brief Creates a specific ARP spoofing message.
 *
 * This function constructs an ARP message to be used for a targeted ARP spoofing attack
 * (man-in-the-middle). It sets the source IP and MAC to the attacker's fake identity
 * (e.g., the router's IP and the attacker's MAC) and the destination to a specific target device.
 * This is used to poison the ARP cache of a single host.
 *
 * @param buffer A pointer to the buffer where the ARP message will be constructed.
 * @param ip_src A pointer to the 4-byte source IP address (e.g., the router's IP).
 * @param mac_src A pointer to the 6-byte source MAC address (e.g., the attacker's MAC).
 * @param ip_dst A pointer to the 4-byte destination IP address (e.g., the victim's IP).
 * @param mac_dst A pointer to the 6-byte destination MAC address (e.g., the victim's MAC).
 * @param len A pointer to a `uint16_t` that will be updated with the total length of the ARP message.
 */
void arp_set_message_attack(
    uint8_t* buffer,
    uint8_t* ip_src,
    uint8_t* mac_src,
    uint8_t* ip_dst,
    uint8_t* mac_dst,
    uint16_t* len);

/**
 * @brief Sends an ARP spoofing packet over the network.
 *
 * This function sends an Arp spoofing packet 120 times per seconds
 *
 * @param ethernet A pointer to the ENC28J60 driver instance.
 * @param buffer A pointer to the buffer containing the complete ARP spoofing packet.
 * @param len The total length of the packet in bytes.
 */
void send_arp_spoofing(enc28j60_t* ethernet, uint8_t* buffer, uint16_t len);

/**
 * @brief Scans the network for active devices using ARP requests.
 *
 * This function sends out a series of ARP requests to a specified range of IP addresses
 * to discover all active devices on the local network segment. It then populates a provided
 * `arp_list` with the IP and MAC addresses of the devices that respond.
 *
 * @param scanner A pointer to a scanner_session_t (forward-declared as
 *                struct ScannerSession to avoid pulling scanner_session.h).
 * @param list A pointer to an `arp_list` structure to store the discovered devices.
 * @param init_ip A pointer to the 4-byte starting IP address for the scan.
 * @param list_count A pointer to a `uint8_t` that will be updated with the number of devices found.
 * @param range The number of IP addresses to scan from the `init_ip`.
 */
struct ScannerSession; // forward declaration for arp_scan_network
void arp_scan_network(
    struct ScannerSession* scanner,
    arp_list* list,
    uint8_t init_ip[4],
    uint8_t* list_count,
    uint8_t range);

/**
 * Set the file-static "my MAC" / "my IP" used by the ARP request/reply
 * builders. Call before set_arp_request / arp_reply_requested if the
 * caller didn't go through arp_get_specific_mac.
 */
void arp_set_my_mac_address(uint8_t* MAC);
void arp_set_my_ip_address(uint8_t* ip);

/**
 * Build an ARP-request frame for `target_ip`. Caller must have set
 * my_mac/my_ip via the helpers above. Writes the framed packet into
 * `buffer` and its length into `*len`.
 */
bool set_arp_request(uint8_t* buffer, uint16_t* len, uint8_t* target_ip);

/**
 * Predicate ctx for matching ARP replies. F0.4f: shared between
 * arp_scan_network and scanner_resolve_next_hop. The predicate runs in
 * the rx_dispatch thread; on a match it copies the source MAC into
 * `target_mac` as a side effect (via get_arp_reply).
 *
 * `target_ip` and `target_mac` are aliased into caller storage; their
 * lifetime must outlive the wait.
 */
typedef struct {
    uint8_t* target_ip;
    uint8_t* target_mac;
} arp_reply_match_ctx_t;

bool arp_reply_match_predicate(const uint8_t* frame, uint16_t len, void* ctx);

/**
 * @brief Retrieves the MAC address for a specific destination IP address using ARP.
 *
 * This function sends an ARP request for a specific IP address and waits for an
 * ARP reply. Upon receiving the reply, it extracts and returns the corresponding MAC address.
 *
 * @param ethernet A pointer to the ENC28J60 driver instance.
 * @param src_ip A pointer to the 4-byte source IP address.
 * @param dst_ip A pointer to the 4-byte destination IP address whose MAC is being requested.
 * @param mac_dst A pointer to a 6-byte buffer where the discovered MAC address will be stored.
 * @return `true` if the MAC address was successfully retrieved, `false` otherwise (e.g., timeout).
 */
bool arp_get_specific_mac(
    enc28j60_t* ethernet,
    uint8_t* sender_ip,
    uint8_t* target_ip,
    uint8_t* sender_mac,
    uint8_t* target_mac);

/**
 * @brief Checks if an incoming packet is a requested ARP.
 *
 * This function analyzes a received ARP packet to determine if it is a
 * requested packet. It checks for the correct opcode and compares if the destination
 * is the assign IP. It responses with an arp reply using the ethernet MAC
 *
 * @param ethernet A pointer to the ENC28J60 driver instance.
 * @param buffer A pointer to the buffer.
 * @param dst_ip A pointer to the 4-byte IP address that was originally requested.
 * @return `true` if the packet is the expected ARP reply, `false` otherwise.
 */
bool arp_reply_requested(enc28j60_t* ethernet, uint8_t* buffer, uint8_t* dst_ip);

/**
 * @brief Checks if a given IP address already exists in an ARP list.
 *
 * This function iterates through a list of known devices and checks if a
 * specific IP address is already present. This is useful during network scanning
 * to avoid adding duplicate entries.
 *
 * @param ip A pointer to the 4-byte IP address to check.
 * @param list A pointer to the `arp_list` to search.
 * @param total_list The total number of entries currently in the list.
 * @return The index of the duplicated IP in the list, or a value indicating it was not found.
 */
uint8_t is_duplicated_ip(uint8_t* ip, arp_list* list, uint8_t total_list);

void send_arp_gratuitous(enc28j60_t* ethernet, uint8_t* source_mac, uint8_t* source_ip);

#endif
