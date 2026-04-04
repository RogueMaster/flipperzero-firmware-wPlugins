#include "dhcp_discover.h"
#include "../utils/packet_utils.h"

#include <furi.h>
#include <string.h>

#define TAG "DHCP_ANALYZE"

/* DHCP packet offsets */
#define DHCP_OP_OFFSET      0
#define DHCP_HTYPE_OFFSET   1
#define DHCP_HLEN_OFFSET    2
#define DHCP_HOPS_OFFSET    3
#define DHCP_XID_OFFSET     4
#define DHCP_SECS_OFFSET    8
#define DHCP_FLAGS_OFFSET   10
#define DHCP_CIADDR_OFFSET  12
#define DHCP_YIADDR_OFFSET  16
#define DHCP_SIADDR_OFFSET  20
#define DHCP_GIADDR_OFFSET  24
#define DHCP_CHADDR_OFFSET  28
#define DHCP_SNAME_OFFSET   44
#define DHCP_FILE_OFFSET    108
#define DHCP_MAGIC_OFFSET   236
#define DHCP_OPTIONS_OFFSET 240

/* Minimum DHCP packet size */
#define DHCP_MIN_SIZE 240

uint16_t dhcp_build_discover(uint8_t* buf, const uint8_t mac[6], uint32_t xid) {
    furi_assert(buf);

    memset(buf, 0, 548);

    /* BOOTP header */
    buf[DHCP_OP_OFFSET] = 1; /* BOOTREQUEST */
    buf[DHCP_HTYPE_OFFSET] = 1; /* Ethernet */
    buf[DHCP_HLEN_OFFSET] = 6; /* MAC length */
    buf[DHCP_HOPS_OFFSET] = 0;

    /* Transaction ID */
    pkt_write_u32_be(buf + DHCP_XID_OFFSET, xid);

    /* Flags: Broadcast (0x8000) */
    pkt_write_u16_be(buf + DHCP_FLAGS_OFFSET, 0x8000);

    /* Client hardware address (CHADDR) */
    memcpy(buf + DHCP_CHADDR_OFFSET, mac, 6);

    /* Magic cookie: 99.130.83.99 = 0x63825363 */
    pkt_write_u32_be(buf + DHCP_MAGIC_OFFSET, DHCP_MAGIC_COOKIE);

    /* Options */
    uint16_t opt_offset = DHCP_OPTIONS_OFFSET;

    /* Option 53: DHCP Message Type = Discover (1) */
    buf[opt_offset++] = DHCP_OPT_MSG_TYPE;
    buf[opt_offset++] = 1;
    buf[opt_offset++] = DHCP_MSG_DISCOVER;

    /* Option 55: Parameter Request List */
    buf[opt_offset++] = DHCP_OPT_PARAM_LIST;
    buf[opt_offset++] = 10;
    buf[opt_offset++] = DHCP_OPT_SUBNET_MASK; /* 1 */
    buf[opt_offset++] = DHCP_OPT_ROUTER; /* 3 */
    buf[opt_offset++] = DHCP_OPT_DNS; /* 6 */
    buf[opt_offset++] = DHCP_OPT_DOMAIN_NAME; /* 15 */
    buf[opt_offset++] = DHCP_OPT_BROADCAST; /* 28 */
    buf[opt_offset++] = DHCP_OPT_NTP; /* 42 */
    buf[opt_offset++] = DHCP_OPT_LEASE_TIME; /* 51 */
    buf[opt_offset++] = DHCP_OPT_SERVER_ID; /* 54 */
    buf[opt_offset++] = DHCP_OPT_RENEWAL_TIME; /* 58 */
    buf[opt_offset++] = DHCP_OPT_REBINDING_TIME; /* 59 */

    /* Option 61: Client Identifier = 01 + MAC */
    buf[opt_offset++] = DHCP_OPT_CLIENT_ID;
    buf[opt_offset++] = 7;
    buf[opt_offset++] = 0x01; /* Hardware type: Ethernet */
    memcpy(buf + opt_offset, mac, 6);
    opt_offset += 6;

    /* Option 255: End */
    buf[opt_offset++] = DHCP_OPT_END;

    /* Pad to minimum 300 bytes (BOOTP minimum) */
    if(opt_offset < 300) opt_offset = 300;

    return opt_offset;
}

bool dhcp_parse_offer(const uint8_t* buf, uint16_t len, uint32_t xid, DhcpAnalyzeResult* result) {
    furi_assert(buf);
    furi_assert(result);

    if(len < DHCP_OPTIONS_OFFSET + 4) {
        FURI_LOG_W(TAG, "DHCP packet too short: %d bytes", len);
        return false;
    }

    /* Check magic cookie */
    uint32_t cookie = pkt_read_u32_be(buf + DHCP_MAGIC_OFFSET);
    if(cookie != DHCP_MAGIC_COOKIE) {
        FURI_LOG_W(TAG, "Bad DHCP magic cookie: 0x%08lX", (unsigned long)cookie);
        return false;
    }

    /* Check transaction ID */
    uint32_t recv_xid = pkt_read_u32_be(buf + DHCP_XID_OFFSET);
    if(recv_xid != xid) {
        FURI_LOG_D(
            TAG,
            "XID mismatch: expected 0x%08lX got 0x%08lX",
            (unsigned long)xid,
            (unsigned long)recv_xid);
        return false;
    }

    memset(result, 0, sizeof(DhcpAnalyzeResult));
    result->xid = xid;

    /* Extract offered IP (YIADDR) */
    memcpy(result->offered_ip, buf + DHCP_YIADDR_OFFSET, 4);

    /* Parse options */
    uint16_t offset = DHCP_OPTIONS_OFFSET;
    bool is_offer = false;

    while(offset < len) {
        uint8_t opt_code = buf[offset++];
        if(opt_code == DHCP_OPT_END) break;
        if(opt_code == 0) continue; /* Padding */

        if(offset >= len) break;
        uint8_t opt_len = buf[offset++];
        if(offset + opt_len > len) break;

        const uint8_t* opt_data = buf + offset;

        /* Record option in fingerprint */
        if(result->fingerprint_len < DHCP_MAX_FINGERPRINT) {
            result->fingerprint[result->fingerprint_len++] = opt_code;
        }

        switch(opt_code) {
        case DHCP_OPT_MSG_TYPE:
            if(opt_len >= 1 && opt_data[0] == DHCP_MSG_OFFER) {
                is_offer = true;
            }
            break;

        case DHCP_OPT_SUBNET_MASK:
            if(opt_len >= 4) memcpy(result->subnet_mask, opt_data, 4);
            break;

        case DHCP_OPT_ROUTER:
            if(opt_len >= 4) memcpy(result->router, opt_data, 4);
            break;

        case DHCP_OPT_DNS:
            if(opt_len >= 4) memcpy(result->dns_server, opt_data, 4);
            if(opt_len >= 8) memcpy(result->dns_server2, opt_data + 4, 4);
            break;

        case DHCP_OPT_DOMAIN_NAME:
            if(opt_len > 0) {
                uint16_t copy_len = (opt_len < DHCP_MAX_DOMAIN - 1) ? opt_len :
                                                                      DHCP_MAX_DOMAIN - 1;
                memcpy(result->domain_name, opt_data, copy_len);
            }
            break;

        case DHCP_OPT_BROADCAST:
            if(opt_len >= 4) memcpy(result->broadcast, opt_data, 4);
            break;

        case DHCP_OPT_NTP:
            if(opt_len >= 4) memcpy(result->ntp_server, opt_data, 4);
            break;

        case DHCP_OPT_LEASE_TIME:
            if(opt_len >= 4) result->lease_time = pkt_read_u32_be(opt_data);
            break;

        case DHCP_OPT_SERVER_ID:
            if(opt_len >= 4) memcpy(result->server_ip, opt_data, 4);
            break;

        case DHCP_OPT_RENEWAL_TIME:
            if(opt_len >= 4) result->renewal_time = pkt_read_u32_be(opt_data);
            break;

        case DHCP_OPT_REBINDING_TIME:
            if(opt_len >= 4) result->rebinding_time = pkt_read_u32_be(opt_data);
            break;

        default:
            break;
        }

        offset += opt_len;
    }

    if(!is_offer) {
        FURI_LOG_D(TAG, "Not a DHCP Offer");
        return false;
    }

    result->valid = true;
    FURI_LOG_I(
        TAG,
        "DHCP Offer: %d.%d.%d.%d from %d.%d.%d.%d",
        result->offered_ip[0],
        result->offered_ip[1],
        result->offered_ip[2],
        result->offered_ip[3],
        result->server_ip[0],
        result->server_ip[1],
        result->server_ip[2],
        result->server_ip[3]);

    return true;
}

void dhcp_format_result(const DhcpAnalyzeResult* result, char* buf, uint16_t buf_size) {
    furi_assert(result);
    furi_assert(buf);

    char offered_str[16], server_str[16], subnet_str[16], router_str[16];
    char dns_str[16], dns2_str[16], ntp_str[16], bcast_str[16];

    pkt_format_ip(result->offered_ip, offered_str);
    pkt_format_ip(result->server_ip, server_str);
    pkt_format_ip(result->subnet_mask, subnet_str);
    pkt_format_ip(result->router, router_str);
    pkt_format_ip(result->dns_server, dns_str);
    pkt_format_ip(result->dns_server2, dns2_str);
    pkt_format_ip(result->ntp_server, ntp_str);
    pkt_format_ip(result->broadcast, bcast_str);

    /* Build fingerprint string */
    char fp_str[128] = {0};
    uint16_t fp_offset = 0;
    for(uint8_t i = 0; i < result->fingerprint_len && fp_offset < sizeof(fp_str) - 4; i++) {
        if(i > 0) {
            fp_str[fp_offset++] = ',';
        }
        int written =
            snprintf(fp_str + fp_offset, sizeof(fp_str) - fp_offset, "%d", result->fingerprint[i]);
        if(written > 0) fp_offset += written;
    }

    snprintf(
        buf,
        buf_size,
        "=== DHCP Offer ===\n"
        "Server: %s\n"
        "Offered: %s\n"
        "Subnet: %s\n"
        "Gateway: %s\n"
        "DNS1: %s\n"
        "DNS2: %s\n"
        "Domain: %s\n"
        "NTP: %s\n"
        "Bcast: %s\n"
        "Lease: %lu sec\n"
        "Renew: %lu sec\n"
        "Rebind: %lu sec\n"
        "Fingerprint:\n  %s\n",
        server_str,
        offered_str,
        subnet_str,
        router_str,
        dns_str,
        dns2_str,
        result->domain_name[0] ? result->domain_name : "(none)",
        ntp_str,
        bcast_str,
        (unsigned long)result->lease_time,
        (unsigned long)result->renewal_time,
        (unsigned long)result->rebinding_time,
        fp_str);
}
