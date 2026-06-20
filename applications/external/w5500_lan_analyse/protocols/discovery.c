#include "discovery.h"

#include <furi.h>
#include <socket.h>
#include <wizchip_conf.h>
#include <string.h>
#include <stdio.h>

#define TAG "DISCOVERY"

/* mDNS query for _services._dns-sd._udp.local PTR */
static const uint8_t MDNS_QUERY[] = {
    /* Header */
    0x00,
    0x00, /* Transaction ID */
    0x00,
    0x00, /* Flags: standard query */
    0x00,
    0x01, /* QDCOUNT: 1 */
    0x00,
    0x00, /* ANCOUNT */
    0x00,
    0x00, /* NSCOUNT */
    0x00,
    0x00, /* ARCOUNT */
    /* Question: _services._dns-sd._udp.local PTR IN */
    0x09,
    '_',
    's',
    'e',
    'r',
    'v',
    'i',
    'c',
    'e',
    's',
    0x07,
    '_',
    'd',
    'n',
    's',
    '-',
    's',
    'd',
    0x04,
    '_',
    'u',
    'd',
    'p',
    0x05,
    'l',
    'o',
    'c',
    'a',
    'l',
    0x00, /* Root label */
    0x00,
    0x0C, /* QTYPE: PTR */
    0x00,
    0x01, /* QCLASS: IN */
};

/* SSDP M-SEARCH request */
static const char SSDP_MSEARCH[] = "M-SEARCH * HTTP/1.1\r\n"
                                   "HOST: 239.255.255.250:1900\r\n"
                                   "MAN: \"ssdp:discover\"\r\n"
                                   "MX: 3\r\n"
                                   "ST: ssdp:all\r\n"
                                   "\r\n";

bool mdns_send_query(uint8_t socket_num) {
    close(socket_num);
    int8_t ret = socket(socket_num, Sn_MR_UDP, MDNS_PORT, 0);
    if(ret != socket_num) {
        FURI_LOG_E(TAG, "mDNS socket open failed: %d", ret);
        return false;
    }

    uint8_t mcast_ip[] = MDNS_MCAST_IP;
    int32_t sent =
        sendto(socket_num, (uint8_t*)MDNS_QUERY, sizeof(MDNS_QUERY), mcast_ip, MDNS_PORT);
    if(sent <= 0) {
        FURI_LOG_E(TAG, "mDNS query send failed: %ld", sent);
        close(socket_num);
        return false;
    }

    FURI_LOG_I(TAG, "mDNS query sent");
    return true;
}

/* Skip a DNS name in buffer, handling compression pointers */
static uint16_t dns_skip_name_disc(const uint8_t* buf, uint16_t len, uint16_t offset) {
    while(offset < len) {
        uint8_t label_len = buf[offset];
        if(label_len == 0) return offset + 1;
        if((label_len & 0xC0) == 0xC0) return offset + 2;
        offset += 1 + label_len;
    }
    return 0;
}

/* Extract a DNS name as readable string (with recursion depth limit) */
static uint16_t dns_read_name_depth(
    const uint8_t* buf,
    uint16_t len,
    uint16_t offset,
    char* out,
    uint16_t out_size,
    uint8_t depth) {
    if(depth > 4) return offset; /* prevent infinite recursion from circular pointers */

    uint16_t pos = offset;
    uint16_t out_pos = 0;
    bool first = true;

    while(pos < len && out_pos < out_size - 1) {
        uint8_t label_len = buf[pos];
        if(label_len == 0) {
            pos++;
            break;
        }
        if((label_len & 0xC0) == 0xC0) {
            /* Pointer - follow it */
            uint16_t ptr = ((uint16_t)(label_len & 0x3F) << 8) | buf[pos + 1];
            dns_read_name_depth(buf, len, ptr, out + out_pos, out_size - out_pos, depth + 1);
            return offset + 2; /* Original position advances by 2 */
        }
        if(!first && out_pos < out_size - 1) {
            out[out_pos++] = '.';
        }
        first = false;
        pos++;
        uint8_t copy_len = label_len;
        if(out_pos + copy_len >= out_size - 1) copy_len = (uint8_t)(out_size - 1 - out_pos);
        memcpy(out + out_pos, buf + pos, copy_len);
        out_pos += copy_len;
        pos += label_len;
    }
    out[out_pos] = '\0';
    return pos;
}

static uint16_t
    dns_read_name(const uint8_t* buf, uint16_t len, uint16_t offset, char* out, uint16_t out_size) {
    return dns_read_name_depth(buf, len, offset, out, out_size, 0);
}

bool mdns_parse_response(
    const uint8_t* buf,
    uint16_t len,
    const uint8_t from_ip[4],
    DiscoveryDevice* device) {
    if(len < 12) return false;

    /* Check it's a response */
    if(!(buf[2] & 0x80)) return false;

    uint16_t ancount = ((uint16_t)buf[6] << 8) | buf[7];
    uint16_t qdcount = ((uint16_t)buf[4] << 8) | buf[5];

    if(ancount == 0) return false;

    /* Skip questions */
    uint16_t pos = 12;
    for(uint16_t i = 0; i < qdcount && pos < len; i++) {
        pos = dns_skip_name_disc(buf, len, pos);
        if(pos == 0 || pos + 4 > len) return false;
        pos += 4;
    }

    /* Parse first answer */
    if(pos >= len) return false;

    char name_buf[DISCOVERY_NAME_LEN];
    memset(name_buf, 0, sizeof(name_buf));
    dns_read_name(buf, len, pos, name_buf, sizeof(name_buf));
    pos = dns_skip_name_disc(buf, len, pos);
    if(pos == 0 || pos + 10 > len) return false;

    uint16_t rtype = ((uint16_t)buf[pos] << 8) | buf[pos + 1];
    uint16_t rdlength = ((uint16_t)buf[pos + 8] << 8) | buf[pos + 9];
    pos += 10;

    memset(device, 0, sizeof(DiscoveryDevice));
    memcpy(device->ip, from_ip, 4);
    device->source = DiscoverySourceMdns;
    device->valid = true;

    if(rtype == 0x0C && pos + rdlength <= len) { /* PTR */
        char ptr_name[DISCOVERY_NAME_LEN];
        memset(ptr_name, 0, sizeof(ptr_name));
        dns_read_name(buf, len, pos, ptr_name, sizeof(ptr_name));
        strncpy(device->name, ptr_name, DISCOVERY_NAME_LEN - 1);
        strncpy(device->service_type, name_buf, DISCOVERY_TYPE_LEN - 1);
    } else {
        strncpy(device->name, name_buf, DISCOVERY_NAME_LEN - 1);
    }

    return true;
}

bool ssdp_send_msearch(uint8_t socket_num) {
    close(socket_num);
    int8_t ret = socket(socket_num, Sn_MR_UDP, 0, 0);
    if(ret != socket_num) {
        FURI_LOG_E(TAG, "SSDP socket open failed: %d", ret);
        return false;
    }

    uint8_t mcast_ip[] = SSDP_MCAST_IP;
    int32_t sent =
        sendto(socket_num, (uint8_t*)SSDP_MSEARCH, strlen(SSDP_MSEARCH), mcast_ip, SSDP_PORT);
    if(sent <= 0) {
        FURI_LOG_E(TAG, "SSDP M-SEARCH send failed: %ld", sent);
        close(socket_num);
        return false;
    }

    FURI_LOG_I(TAG, "SSDP M-SEARCH sent");
    return true;
}

/* Extract a header value from HTTP response */
static bool ssdp_extract_header(
    const char* buf,
    uint16_t len,
    const char* header,
    char* value,
    uint16_t value_size) {
    uint16_t hdr_len = strlen(header);
    for(uint16_t i = 0; i + hdr_len < len; i++) {
        /* Case-insensitive header match at start of line */
        if(i == 0 || buf[i - 1] == '\n') {
            bool match = true;
            for(uint16_t j = 0; j < hdr_len; j++) {
                char a = buf[i + j];
                char b = header[j];
                /* Simple ASCII tolower */
                if(a >= 'A' && a <= 'Z') a += 32;
                if(b >= 'A' && b <= 'Z') b += 32;
                if(a != b) {
                    match = false;
                    break;
                }
            }
            if(match) {
                uint16_t start = i + hdr_len;
                /* Skip whitespace */
                while(start < len && (buf[start] == ' ' || buf[start] == '\t'))
                    start++;
                /* Copy until CR/LF */
                uint16_t vpos = 0;
                while(start < len && buf[start] != '\r' && buf[start] != '\n' &&
                      vpos < value_size - 1) {
                    value[vpos++] = buf[start++];
                }
                value[vpos] = '\0';
                return vpos > 0;
            }
        }
    }
    return false;
}

bool ssdp_parse_response(
    const uint8_t* buf,
    uint16_t len,
    const uint8_t from_ip[4],
    DiscoveryDevice* device) {
    if(len < 10) return false;

    memset(device, 0, sizeof(DiscoveryDevice));
    memcpy(device->ip, from_ip, 4);
    device->source = DiscoverySourceSsdp;

    char server[DISCOVERY_NAME_LEN];
    char st[DISCOVERY_TYPE_LEN];

    bool has_server =
        ssdp_extract_header((const char*)buf, len, "SERVER:", server, sizeof(server));
    bool has_st = ssdp_extract_header((const char*)buf, len, "ST:", st, sizeof(st));

    if(!has_server && !has_st) return false;

    if(has_server) {
        strncpy(device->name, server, DISCOVERY_NAME_LEN - 1);
    } else {
        strncpy(device->name, "Unknown", DISCOVERY_NAME_LEN - 1);
    }

    if(has_st) {
        strncpy(device->service_type, st, DISCOVERY_TYPE_LEN - 1);
    }

    device->valid = true;
    return true;
}
