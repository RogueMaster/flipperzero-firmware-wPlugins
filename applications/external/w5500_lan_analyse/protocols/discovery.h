#pragma once

#include <stdint.h>
#include <stdbool.h>

/* mDNS multicast address and port */
#define MDNS_MCAST_IP {224, 0, 0, 251}
#define MDNS_PORT     5353

/* SSDP multicast address and port */
#define SSDP_MCAST_IP {239, 255, 255, 250}
#define SSDP_PORT     1900

/* Socket assignments */
#define W5500_MDNS_SOCKET 3
#define W5500_SSDP_SOCKET 4

/* Discovery timeout in ms */
#define DISCOVERY_TIMEOUT_MS 5000

/* Max discovered devices */
#define DISCOVERY_MAX_DEVICES 32

/* Max string lengths */
#define DISCOVERY_NAME_LEN 48
#define DISCOVERY_TYPE_LEN 32

/* Discovery source type */
typedef enum {
    DiscoverySourceMdns,
    DiscoverySourceSsdp,
} DiscoverySource;

/* Discovered device */
typedef struct {
    char name[DISCOVERY_NAME_LEN];
    char service_type[DISCOVERY_TYPE_LEN];
    uint8_t ip[4];
    DiscoverySource source;
    bool valid;
} DiscoveryDevice;

/**
 * Send mDNS service discovery query.
 * socket_num: W5500 UDP socket
 * Returns true if query was sent.
 */
bool mdns_send_query(uint8_t socket_num);

/**
 * Parse an mDNS response and extract device info.
 * buf: UDP payload
 * len: payload length
 * from_ip: source IP
 * device: output
 * Returns true if a device was parsed.
 */
bool mdns_parse_response(
    const uint8_t* buf,
    uint16_t len,
    const uint8_t from_ip[4],
    DiscoveryDevice* device);

/**
 * Send SSDP M-SEARCH request.
 * socket_num: W5500 UDP socket
 * Returns true if request was sent.
 */
bool ssdp_send_msearch(uint8_t socket_num);

/**
 * Parse an SSDP response and extract device info.
 * buf: UDP payload (HTTP response)
 * len: payload length
 * from_ip: source IP
 * device: output
 * Returns true if a device was parsed.
 */
bool ssdp_parse_response(
    const uint8_t* buf,
    uint16_t len,
    const uint8_t from_ip[4],
    DiscoveryDevice* device);
