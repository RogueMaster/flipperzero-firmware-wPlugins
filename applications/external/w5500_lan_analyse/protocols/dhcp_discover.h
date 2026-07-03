#pragma once

#include <stdint.h>
#include <stdbool.h>

/* DHCP ports */
#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68

/* DHCP message types */
#define DHCP_MSG_DISCOVER 1
#define DHCP_MSG_OFFER    2
#define DHCP_MSG_REQUEST  3
#define DHCP_MSG_ACK      5

/* DHCP magic cookie */
#define DHCP_MAGIC_COOKIE 0x63825363

/* Common DHCP option codes */
#define DHCP_OPT_SUBNET_MASK    1
#define DHCP_OPT_ROUTER         3
#define DHCP_OPT_DNS            6
#define DHCP_OPT_DOMAIN_NAME    15
#define DHCP_OPT_BROADCAST      28
#define DHCP_OPT_NTP            42
#define DHCP_OPT_LEASE_TIME     51
#define DHCP_OPT_MSG_TYPE       53
#define DHCP_OPT_SERVER_ID      54
#define DHCP_OPT_PARAM_LIST     55
#define DHCP_OPT_RENEWAL_TIME   58
#define DHCP_OPT_REBINDING_TIME 59
#define DHCP_OPT_CLIENT_ID      61
#define DHCP_OPT_END            255

/* Max fingerprint option entries */
#define DHCP_MAX_FINGERPRINT 32

/* Max domain name length */
#define DHCP_MAX_DOMAIN 64

typedef struct {
    /* Offered network info */
    uint8_t offered_ip[4];
    uint8_t server_ip[4];
    uint8_t subnet_mask[4];
    uint8_t router[4];
    uint8_t dns_server[4];
    uint8_t dns_server2[4];
    uint8_t broadcast[4];
    uint8_t ntp_server[4];
    char domain_name[DHCP_MAX_DOMAIN];

    /* Timing */
    uint32_t lease_time;
    uint32_t renewal_time;
    uint32_t rebinding_time;

    /* DHCP fingerprint: order of options in the Offer */
    uint8_t fingerprint[DHCP_MAX_FINGERPRINT];
    uint8_t fingerprint_len;

    /* Transaction ID */
    uint32_t xid;

    bool valid;
} DhcpAnalyzeResult;

/**
 * Build a DHCP Discover packet.
 * buf: output buffer (must be at least 548 bytes)
 * mac: our MAC address
 * xid: transaction ID (random)
 * Returns packet length.
 */
uint16_t dhcp_build_discover(uint8_t* buf, const uint8_t mac[6], uint32_t xid);

/**
 * Parse a DHCP Offer packet.
 * buf: received UDP payload (DHCP message)
 * len: payload length
 * xid: expected transaction ID
 * result: output structure
 * Returns true if valid DHCP Offer parsed successfully.
 */
bool dhcp_parse_offer(const uint8_t* buf, uint16_t len, uint32_t xid, DhcpAnalyzeResult* result);

/**
 * Format DHCP result into human-readable string.
 */
void dhcp_format_result(const DhcpAnalyzeResult* result, char* buf, uint16_t buf_size);
