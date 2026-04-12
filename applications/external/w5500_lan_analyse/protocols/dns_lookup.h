#pragma once

#include <stdint.h>
#include <stdbool.h>

/* DNS port */
#define DNS_SERVER_PORT 53

/* DNS socket on W5500 */
#define W5500_DNS_SOCKET 3

/* DNS query timeout in ms */
#define DNS_TIMEOUT_MS 3000

/* Maximum hostname length */
#define DNS_MAX_HOSTNAME 64

/* DNS response codes */
#define DNS_RCODE_OK       0
#define DNS_RCODE_NXDOMAIN 3

/* DNS result */
typedef struct {
    uint8_t resolved_ip[4];
    uint8_t rcode;
    bool success;
} DnsLookupResult;

/**
 * Perform a DNS A-record lookup using a W5500 UDP socket.
 * socket_num: W5500 socket to use
 * dns_server: DNS server IP (from DHCP)
 * hostname: null-terminated hostname (e.g. "google.com")
 * result: output
 * Returns true if an A record was resolved.
 */
bool dns_lookup(
    uint8_t socket_num,
    const uint8_t dns_server[4],
    const char* hostname,
    DnsLookupResult* result);
