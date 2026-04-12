#include "dns_lookup.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_random.h>
#include <socket.h>
#include <wizchip_conf.h>
#include <string.h>

#define TAG "DNS"

/* DNS header size */
#define DNS_HEADER_SIZE 12

/* DNS record types */
#define DNS_TYPE_A   1
#define DNS_CLASS_IN 1

/* Max DNS packet size */
#define DNS_BUF_SIZE 512

/**
 * Encode a hostname into DNS QNAME format.
 * "google.com" -> "\x06google\x03com\x00"
 * Returns number of bytes written, 0 on error.
 */
static uint16_t dns_encode_qname(uint8_t* buf, uint16_t buf_size, const char* hostname) {
    uint16_t pos = 0;
    const char* ptr = hostname;

    while(*ptr) {
        /* Find next dot or end */
        const char* dot = ptr;
        while(*dot && *dot != '.')
            dot++;

        uint8_t label_len = (uint8_t)(dot - ptr);
        if(label_len == 0 || label_len > 63) return 0;
        if(pos + 1 + label_len >= buf_size) return 0;

        buf[pos++] = label_len;
        memcpy(&buf[pos], ptr, label_len);
        pos += label_len;

        ptr = (*dot == '.') ? dot + 1 : dot;
    }

    if(pos + 1 >= buf_size) return 0;
    buf[pos++] = 0; /* Root label */
    return pos;
}

/**
 * Build a DNS query packet for A record.
 * Returns packet length, 0 on error.
 */
static uint16_t
    dns_build_query(uint8_t* buf, uint16_t buf_size, const char* hostname, uint16_t tx_id) {
    if(buf_size < DNS_HEADER_SIZE + 4) return 0;

    memset(buf, 0, DNS_HEADER_SIZE);

    /* Header */
    buf[0] = (uint8_t)(tx_id >> 8);
    buf[1] = (uint8_t)(tx_id & 0xFF);
    buf[2] = 0x01; /* RD (Recursion Desired) */
    buf[3] = 0x00;
    buf[4] = 0x00; /* QDCOUNT = 1 */
    buf[5] = 0x01;
    /* ANCOUNT, NSCOUNT, ARCOUNT = 0 */

    /* Question section */
    uint16_t qname_len =
        dns_encode_qname(&buf[DNS_HEADER_SIZE], buf_size - DNS_HEADER_SIZE - 4, hostname);
    if(qname_len == 0) return 0;

    uint16_t pos = DNS_HEADER_SIZE + qname_len;

    /* QTYPE = A (1) */
    buf[pos++] = 0x00;
    buf[pos++] = DNS_TYPE_A;

    /* QCLASS = IN (1) */
    buf[pos++] = 0x00;
    buf[pos++] = DNS_CLASS_IN;

    return pos;
}

/**
 * Skip a DNS name in the response (handles pointer compression).
 * Returns new offset, 0 on error.
 */
static uint16_t dns_skip_name(const uint8_t* buf, uint16_t len, uint16_t offset) {
    uint16_t pos = offset;
    bool jumped = false;

    while(pos < len) {
        uint8_t label_len = buf[pos];

        if(label_len == 0) {
            /* End of name */
            return jumped ? offset + 2 : pos + 1;
        }

        if((label_len & 0xC0) == 0xC0) {
            /* Pointer compression */
            if(!jumped) {
                offset = pos; /* Remember where to continue */
            }
            return offset + 2;
        }

        pos += 1 + label_len;
    }

    return 0; /* Error */
}

/**
 * Parse DNS response and extract first A record.
 */
static bool
    dns_parse_response(const uint8_t* buf, uint16_t len, uint16_t tx_id, DnsLookupResult* result) {
    if(len < DNS_HEADER_SIZE) return false;

    /* Check transaction ID */
    uint16_t resp_id = ((uint16_t)buf[0] << 8) | buf[1];
    if(resp_id != tx_id) return false;

    /* Check QR bit (must be response) */
    if(!(buf[2] & 0x80)) return false;

    /* Get RCODE */
    result->rcode = buf[3] & 0x0F;
    if(result->rcode != DNS_RCODE_OK) {
        return false;
    }

    /* Get counts */
    uint16_t qdcount = ((uint16_t)buf[4] << 8) | buf[5];
    uint16_t ancount = ((uint16_t)buf[6] << 8) | buf[7];

    if(ancount == 0) return false;

    /* Skip question section */
    uint16_t pos = DNS_HEADER_SIZE;
    for(uint16_t i = 0; i < qdcount; i++) {
        pos = dns_skip_name(buf, len, pos);
        if(pos == 0 || pos + 4 > len) return false;
        pos += 4; /* Skip QTYPE + QCLASS */
    }

    /* Parse answer section - find first A record */
    for(uint16_t i = 0; i < ancount; i++) {
        pos = dns_skip_name(buf, len, pos);
        if(pos == 0 || pos + 10 > len) return false;

        uint16_t rtype = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
        /* uint16_t rclass = ((uint16_t)buf[pos + 2] << 8) | buf[pos + 3]; */
        /* uint32_t ttl at pos+4..pos+7 */
        uint16_t rdlength = ((uint16_t)buf[pos + 8] << 8) | buf[pos + 9];
        pos += 10;

        if(pos + rdlength > len) return false;

        if(rtype == DNS_TYPE_A && rdlength == 4) {
            memcpy(result->resolved_ip, &buf[pos], 4);
            result->success = true;
            return true;
        }

        pos += rdlength;
    }

    return false;
}

bool dns_lookup(
    uint8_t socket_num,
    const uint8_t dns_server[4],
    const char* hostname,
    DnsLookupResult* result) {
    furi_assert(result);
    furi_assert(hostname);

    memset(result, 0, sizeof(DnsLookupResult));

    /* Generate random transaction ID */
    uint16_t tx_id;
    furi_hal_random_fill_buf((uint8_t*)&tx_id, sizeof(tx_id));

    /* Build DNS query */
    uint8_t dns_buf[DNS_BUF_SIZE];
    uint16_t query_len = dns_build_query(dns_buf, DNS_BUF_SIZE, hostname, tx_id);
    if(query_len == 0) {
        FURI_LOG_E(TAG, "Failed to build DNS query for '%s'", hostname);
        return false;
    }

    /* Open UDP socket */
    close(socket_num);
    int8_t ret = socket(socket_num, Sn_MR_UDP, 0, 0);
    if(ret != socket_num) {
        FURI_LOG_E(TAG, "Failed to open UDP socket: %d", ret);
        return false;
    }

    /* Send query to DNS server */
    int32_t sent = sendto(socket_num, dns_buf, query_len, (uint8_t*)dns_server, DNS_SERVER_PORT);
    if(sent <= 0) {
        FURI_LOG_E(TAG, "Failed to send DNS query: %ld", sent);
        close(socket_num);
        return false;
    }

    FURI_LOG_I(TAG, "DNS query sent for '%s' (txid=0x%04X)", hostname, tx_id);

    /* Wait for response */
    uint32_t start_tick = furi_get_tick();
    uint8_t from_ip[4];
    uint16_t from_port;

    while(furi_get_tick() - start_tick < DNS_TIMEOUT_MS) {
        uint16_t rx_size = getSn_RX_RSR(socket_num);
        if(rx_size > 0) {
            int32_t received = recvfrom(socket_num, dns_buf, DNS_BUF_SIZE, from_ip, &from_port);
            if(received > 0) {
                /* Verify response is from the expected DNS server */
                if(memcmp(from_ip, dns_server, 4) != 0) continue;
                if(dns_parse_response(dns_buf, (uint16_t)received, tx_id, result)) {
                    FURI_LOG_I(
                        TAG,
                        "Resolved '%s' -> %d.%d.%d.%d",
                        hostname,
                        result->resolved_ip[0],
                        result->resolved_ip[1],
                        result->resolved_ip[2],
                        result->resolved_ip[3]);
                    close(socket_num);
                    return true;
                }
            }
        }
        furi_delay_ms(10);
    }

    FURI_LOG_W(TAG, "DNS lookup timeout for '%s'", hostname);
    close(socket_num);
    return false;
}
