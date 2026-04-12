#include "netbios_query.h"
#include <furi.h>
#include <socket.h>
#include <string.h>

#define NBNS_SOCK       3
#define NBNS_PORT       137
#define NBNS_LOCAL_PORT 13700
#define NBNS_TIMEOUT_MS 3000

/* NetBIOS name suffix types */
#define NB_SUFFIX_WORKSTATION 0x00
#define NB_SUFFIX_DOMAIN      0x00 /* group */
#define NB_SUFFIX_SERVER      0x20

static uint16_t read_u16_be(const uint8_t* p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static void write_u16_be(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v);
}

/**
 * Build NetBIOS Node Status Request.
 * Query name "*" (wildcard) to get all registered names.
 */
static uint16_t nbns_build_node_status(uint8_t* pkt, uint16_t pkt_size, uint16_t txn_id) {
    if(pkt_size < 50) return 0;
    uint16_t idx = 0;

    /* Header (12 bytes) */
    write_u16_be(&pkt[idx], txn_id);
    idx += 2;
    write_u16_be(&pkt[idx], 0x0000); /* flags: query, opcode=0 */
    idx += 2;
    write_u16_be(&pkt[idx], 1); /* QDCOUNT = 1 */
    idx += 2;
    write_u16_be(&pkt[idx], 0); /* ANCOUNT */
    idx += 2;
    write_u16_be(&pkt[idx], 0); /* NSCOUNT */
    idx += 2;
    write_u16_be(&pkt[idx], 0); /* ARCOUNT */
    idx += 2;

    /* Question: encoded name "*" (wildcard) */
    /* First-level encoding: 32 bytes for 16-char name
     * '*' = 0x2A -> 'C' 'K' (each nibble + 'A')
     * padding with ' ' (0x20) -> 'C' 'A'
     */
    pkt[idx++] = 0x20; /* length = 32 */
    /* Encode '*' followed by 15 spaces (NUL suffix) */
    uint8_t raw_name[16];
    memset(raw_name, 0x20, 16); /* fill with spaces */
    raw_name[0] = '*';
    raw_name[15] = 0x00; /* suffix for wildcard */

    for(int i = 0; i < 16; i++) {
        pkt[idx++] = 'A' + ((raw_name[i] >> 4) & 0x0F);
        pkt[idx++] = 'A' + (raw_name[i] & 0x0F);
    }
    pkt[idx++] = 0x00; /* end of name */

    /* QTYPE = NBSTAT (0x0021) */
    write_u16_be(&pkt[idx], 0x0021);
    idx += 2;
    /* QCLASS = IN (0x0001) */
    write_u16_be(&pkt[idx], 0x0001);
    idx += 2;

    return idx;
}

/**
 * Parse NetBIOS Node Status Response.
 */
static void nbns_parse_response(
    const uint8_t* buf,
    uint16_t len,
    uint16_t txn_id,
    NetbiosQueryResult* result) {
    if(len < 12) return;

    /* Check transaction ID */
    uint16_t resp_txn = read_u16_be(&buf[0]);
    if(resp_txn != txn_id) return;

    /* Check flags: response bit set */
    uint16_t flags = read_u16_be(&buf[2]);
    if(!(flags & 0x8000)) return; /* not a response */

    /* Skip header (12 bytes) */
    uint16_t idx = 12;

    /* Skip question section if QDCOUNT > 0 */
    /* Skip the encoded name */
    while(idx < len && buf[idx] != 0x00) {
        uint8_t label_len = buf[idx];
        if(label_len >= 0xC0) {
            idx += 2; /* compression pointer */
            break;
        }
        idx += 1 + label_len;
    }
    if(idx < len && buf[idx] == 0x00) idx++; /* null terminator */
    idx += 4; /* skip QTYPE + QCLASS */

    /* Answer section: skip name (possibly compressed) */
    if(idx >= len) return;
    if(buf[idx] >= 0xC0) {
        idx += 2; /* compression pointer */
    } else {
        while(idx < len && buf[idx] != 0x00) {
            uint8_t label_len = buf[idx];
            idx += 1 + label_len;
        }
        if(idx < len) idx++; /* null terminator */
    }

    /* TYPE (2) + CLASS (2) + TTL (4) + RDLENGTH (2) = 10 bytes */
    if(idx + 10 > len) return;
    idx += 8; /* skip TYPE + CLASS + TTL */
    uint16_t rdlength = read_u16_be(&buf[idx]);
    idx += 2;

    if(idx + rdlength > len) return;

    /* RDATA: num_names (1 byte) + name entries (18 bytes each) + statistics (varies) */
    if(rdlength < 1) return;
    uint8_t num_names = buf[idx++];
    if(num_names > NETBIOS_MAX_NAMES) num_names = NETBIOS_MAX_NAMES;

    result->computer_name[0] = '\0';
    result->workgroup[0] = '\0';

    for(uint8_t i = 0; i < num_names; i++) {
        if(idx + 18 > len) break;

        /* Name: 15 bytes + 1 suffix byte */
        NetbiosName* n = &result->names[result->name_count];
        memcpy(n->name, &buf[idx], 15);
        n->name[15] = '\0';
        /* Trim trailing spaces */
        for(int j = 14; j >= 0; j--) {
            if(n->name[j] == ' ' || n->name[j] == '\0')
                n->name[j] = '\0';
            else
                break;
        }
        n->suffix = buf[idx + 15];
        idx += 16;

        /* Flags */
        n->flags = read_u16_be(&buf[idx]);
        idx += 2;
        n->is_group = (n->flags & 0x8000) != 0;

        /* Extract computer name (first unique workstation name) */
        if(!n->is_group && n->suffix == NB_SUFFIX_WORKSTATION &&
           result->computer_name[0] == '\0') {
            memcpy(result->computer_name, n->name, 16);
        }

        /* Extract workgroup (first group name) */
        if(n->is_group && n->suffix == NB_SUFFIX_DOMAIN && result->workgroup[0] == '\0') {
            memcpy(result->workgroup, n->name, 16);
        }

        result->name_count++;
    }

    /* Unit ID (MAC address) - 6 bytes after name table */
    if(idx + 6 <= (uint16_t)(len)) {
        memcpy(result->unit_id, &buf[idx], 6);
        result->has_unit_id = true;
    }

    result->valid = (result->name_count > 0);
}

bool netbios_node_status(const uint8_t target_ip[4], NetbiosQueryResult* result) {
    memset(result, 0, sizeof(NetbiosQueryResult));

    close(NBNS_SOCK);
    if(socket(NBNS_SOCK, Sn_MR_UDP, NBNS_LOCAL_PORT, 0) != NBNS_SOCK) return false;

    uint16_t txn_id = (uint16_t)(furi_get_tick() & 0xFFFF);
    uint8_t pkt[64];
    uint16_t pkt_len = nbns_build_node_status(pkt, sizeof(pkt), txn_id);
    if(pkt_len == 0) {
        close(NBNS_SOCK);
        return false;
    }

    if(sendto(NBNS_SOCK, pkt, pkt_len, (uint8_t*)target_ip, NBNS_PORT) <= 0) {
        close(NBNS_SOCK);
        return false;
    }

    uint8_t* rx_buf = malloc(512);
    if(!rx_buf) {
        close(NBNS_SOCK);
        return false;
    }

    uint32_t start = furi_get_tick();
    bool got_reply = false;

    while((furi_get_tick() - start) < NBNS_TIMEOUT_MS) {
        uint16_t rx_len = getSn_RX_RSR(NBNS_SOCK);
        if(rx_len > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t recv_len = recvfrom(NBNS_SOCK, rx_buf, 512, from_ip, &from_port);
            if(recv_len > 0) {
                nbns_parse_response(rx_buf, (uint16_t)recv_len, txn_id, result);
                if(result->valid) {
                    got_reply = true;
                    break;
                }
            }
        }
        furi_delay_ms(10);
    }

    free(rx_buf);
    close(NBNS_SOCK);
    return got_reply;
}
