#include "dns_poison.h"
#include <furi.h>
#include <socket.h>
#include <string.h>

#define DNS_SOCK       3
#define DNS_PORT       53
#define DNS_LOCAL_PORT 15300
#define DNS_TIMEOUT_MS 3000

static uint16_t read_u16_be(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static void write_u16_be(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

/**
 * Build a minimal DNS A-record query.
 * Returns packet length.
 */
static uint16_t
    dns_build_query(uint8_t* pkt, uint16_t pkt_size, const char* hostname, uint16_t txn_id) {
    uint16_t idx = 0;

    /* Header */
    if(pkt_size < 64) return 0;
    write_u16_be(&pkt[idx], txn_id);
    idx += 2;
    write_u16_be(&pkt[idx], 0x0100); /* flags: standard query, RD=1 */
    idx += 2;
    write_u16_be(&pkt[idx], 1); /* QDCOUNT */
    idx += 2;
    write_u16_be(&pkt[idx], 0); /* ANCOUNT */
    idx += 2;
    write_u16_be(&pkt[idx], 0); /* NSCOUNT */
    idx += 2;
    write_u16_be(&pkt[idx], 0); /* ARCOUNT */
    idx += 2;

    /* Question: encode hostname as labels */
    const char* p = hostname;
    while(*p) {
        const char* dot = strchr(p, '.');
        uint8_t label_len;
        if(dot) {
            label_len = (uint8_t)(dot - p);
        } else {
            label_len = (uint8_t)strlen(p);
        }
        if(idx + 1 + label_len >= pkt_size - 5) return 0;
        pkt[idx++] = label_len;
        memcpy(&pkt[idx], p, label_len);
        idx += label_len;
        p += label_len;
        if(*p == '.') p++;
    }
    pkt[idx++] = 0x00; /* end of name */

    write_u16_be(&pkt[idx], 0x0001); /* QTYPE = A */
    idx += 2;
    write_u16_be(&pkt[idx], 0x0001); /* QCLASS = IN */
    idx += 2;

    return idx;
}

/**
 * Parse DNS response, extract A-record IPs.
 */
static uint8_t dns_parse_response(
    const uint8_t* buf,
    uint16_t len,
    uint16_t txn_id,
    uint8_t addrs[][4],
    uint8_t max_addrs) {
    if(len < 12) return 0;

    /* Verify transaction ID */
    if(read_u16_be(&buf[0]) != txn_id) return 0;

    /* Check response flag */
    uint16_t flags = read_u16_be(&buf[2]);
    if(!(flags & 0x8000)) return 0;

    /* Check RCODE */
    if((flags & 0x000F) != 0) return 0;

    uint16_t qdcount = read_u16_be(&buf[4]);
    uint16_t ancount = read_u16_be(&buf[6]);

    /* Skip question section */
    uint16_t idx = 12;
    for(uint16_t q = 0; q < qdcount; q++) {
        while(idx < len) {
            if(buf[idx] == 0x00) {
                idx++;
                break;
            }
            if((buf[idx] & 0xC0) == 0xC0) {
                idx += 2;
                break;
            }
            idx += 1 + buf[idx];
        }
        idx += 4; /* QTYPE + QCLASS */
    }

    /* Parse answers */
    uint8_t count = 0;
    for(uint16_t a = 0; a < ancount && count < max_addrs; a++) {
        if(idx >= len) break;

        /* Skip name (possibly compressed) */
        if((buf[idx] & 0xC0) == 0xC0) {
            idx += 2;
        } else {
            while(idx < len && buf[idx] != 0x00) {
                if((buf[idx] & 0xC0) == 0xC0) {
                    idx += 2;
                    goto name_done;
                }
                idx += 1 + buf[idx];
            }
            if(idx < len) idx++;
        }
    name_done:

        if(idx + 10 > len) break;
        uint16_t rtype = read_u16_be(&buf[idx]);
        idx += 2;
        idx += 2; /* RCLASS */
        idx += 4; /* TTL */
        uint16_t rdlength = read_u16_be(&buf[idx]);
        idx += 2;

        if(rtype == 0x0001 && rdlength == 4 && idx + 4 <= len) {
            memcpy(addrs[count], &buf[idx], 4);
            count++;
        }
        idx += rdlength;
    }

    return count;
}

/**
 * Resolve hostname via a specific DNS server.
 */
static uint8_t dns_resolve_via(
    const char* hostname,
    const uint8_t dns_ip[4],
    uint8_t addrs[][4],
    uint8_t max_addrs,
    uint16_t local_port) {
    close(DNS_SOCK);
    if(socket(DNS_SOCK, Sn_MR_UDP, local_port, 0) != DNS_SOCK) return 0;

    uint16_t txn_id = (uint16_t)(furi_get_tick() & 0xFFFF);
    uint8_t pkt[256];
    uint16_t pkt_len = dns_build_query(pkt, sizeof(pkt), hostname, txn_id);
    if(pkt_len == 0) {
        close(DNS_SOCK);
        return 0;
    }

    if(sendto(DNS_SOCK, pkt, pkt_len, (uint8_t*)dns_ip, DNS_PORT) <= 0) {
        close(DNS_SOCK);
        return 0;
    }

    uint32_t start = furi_get_tick();
    uint8_t count = 0;

    while((furi_get_tick() - start) < DNS_TIMEOUT_MS) {
        uint16_t rx_len = getSn_RX_RSR(DNS_SOCK);
        if(rx_len > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t recv_len = recvfrom(DNS_SOCK, pkt, sizeof(pkt), from_ip, &from_port);
            if(recv_len > 0) {
                count = dns_parse_response(pkt, (uint16_t)recv_len, txn_id, addrs, max_addrs);
                break;
            }
        }
        furi_delay_ms(10);
    }

    close(DNS_SOCK);
    return count;
}

bool dns_poison_check(
    const char* hostname,
    const uint8_t local_dns[4],
    const uint8_t public_dns[4],
    DnsPoisonResult* result) {
    memset(result, 0, sizeof(DnsPoisonResult));

    /* Query local DNS */
    result->local_count = dns_resolve_via(
        hostname, local_dns, result->local_addrs, DNS_POISON_MAX_ADDRS, DNS_LOCAL_PORT);
    result->local_ok = (result->local_count > 0);

    /* Small delay between queries */
    furi_delay_ms(100);

    /* Query public DNS */
    result->public_count = dns_resolve_via(
        hostname, public_dns, result->public_addrs, DNS_POISON_MAX_ADDRS, DNS_LOCAL_PORT + 1);
    result->public_ok = (result->public_count > 0);

    result->valid = result->local_ok || result->public_ok;

    if(result->local_ok && result->public_ok) {
        /* Check for any matching addresses */
        result->match = false;
        for(uint8_t i = 0; i < result->local_count; i++) {
            for(uint8_t j = 0; j < result->public_count; j++) {
                if(memcmp(result->local_addrs[i], result->public_addrs[j], 4) == 0) {
                    result->match = true;
                    break;
                }
            }
            if(result->match) break;
        }
        result->mismatch = !result->match;
    }

    return result->valid;
}
