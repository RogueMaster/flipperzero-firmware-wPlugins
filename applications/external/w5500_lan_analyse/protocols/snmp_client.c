#include "snmp_client.h"
#include <furi.h>
#include <socket.h>
#include <string.h>

#define SNMP_SOCK       3
#define SNMP_PORT       161
#define SNMP_LOCAL_PORT 16100
#define SNMP_TIMEOUT_MS 3000

/* ASN.1 / BER types */
#define ASN_SEQUENCE  0x30
#define ASN_INTEGER   0x02
#define ASN_OCTET_STR 0x04
#define ASN_NULL      0x05
#define ASN_OBJ_ID    0x06
#define ASN_GET_REQ   0xa0
#define ASN_GET_RESP  0xa2
#define ASN_TIMETICKS 0x43

/* Standard OIDs (encoded BER) */
/* 1.3.6.1.2.1.1.1.0 = sysDescr */
static const uint8_t oid_sys_descr[] = {0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00};
/* 1.3.6.1.2.1.1.3.0 = sysUpTime */
static const uint8_t oid_sys_uptime[] = {0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x03, 0x00};
/* 1.3.6.1.2.1.1.5.0 = sysName */
static const uint8_t oid_sys_name[] = {0x2b, 0x06, 0x01, 0x02, 0x01, 0x01, 0x05, 0x00};
/* 1.3.6.1.2.1.2.2.1.8.1 = ifOperStatus.1 */
static const uint8_t oid_if_status[] = {0x2b, 0x06, 0x01, 0x02, 0x01, 0x02, 0x02, 0x01, 0x08, 0x01};

/* Write ASN.1 length (short or long form) */
static uint16_t asn_write_length(uint8_t* buf, uint16_t len) {
    if(len < 0x80) {
        buf[0] = (uint8_t)len;
        return 1;
    }
    if(len <= 0xFF) {
        buf[0] = 0x81;
        buf[1] = (uint8_t)len;
        return 2;
    }
    buf[0] = 0x82;
    buf[1] = (uint8_t)(len >> 8);
    buf[2] = (uint8_t)(len & 0xFF);
    return 3;
}

/* Read ASN.1 length, return bytes consumed */
static uint16_t asn_read_length(const uint8_t* buf, uint16_t* out_len) {
    if(buf[0] < 0x80) {
        *out_len = buf[0];
        return 1;
    }
    if(buf[0] == 0x81) {
        *out_len = buf[1];
        return 2;
    }
    if(buf[0] == 0x82) {
        *out_len = ((uint16_t)buf[1] << 8) | buf[2];
        return 3;
    }
    *out_len = 0;
    return 1;
}

/* Return how many bytes asn_write_length() would produce */
static uint16_t asn_length_size(uint16_t len) {
    if(len < 0x80) return 1;
    if(len <= 0xFF) return 2;
    return 3;
}

/**
 * Build SNMP v1/v2c GET-Request for a single OID.
 * Writes directly into pkt (no intermediate stack arrays).
 * Returns total packet length, or 0 on overflow.
 */
static uint16_t snmp_build_get(
    uint8_t* pkt,
    uint16_t pkt_size,
    const char* community,
    bool use_v2c,
    uint32_t request_id,
    const uint8_t* oid,
    uint8_t oid_len) {
    uint8_t comm_len = (uint8_t)strlen(community);

    /* Pre-compute all content lengths bottom-up */
    uint16_t varbind_len = (2 + oid_len) + 2; /* OID TLV + NULL TLV */
    uint16_t vb_seq_len = 1 + asn_length_size(varbind_len) + varbind_len;
    uint16_t vb_list_len = 1 + asn_length_size(vb_seq_len) + vb_seq_len;
    uint16_t pdu_content_len = 6 + 3 + 3 + vb_list_len; /* reqid+err_s+err_i+vblist */
    uint16_t pdu_len = 1 + asn_length_size(pdu_content_len) + pdu_content_len;
    uint16_t msg_content_len = 3 + (2 + comm_len) + pdu_len; /* ver+comm+pdu */
    uint16_t total = 1 + asn_length_size(msg_content_len) + msg_content_len;

    if(total > pkt_size) return 0;

    uint16_t idx = 0;

    /* Top-level SEQUENCE */
    pkt[idx++] = ASN_SEQUENCE;
    idx += asn_write_length(&pkt[idx], msg_content_len);

    /* Version INTEGER */
    pkt[idx++] = ASN_INTEGER;
    pkt[idx++] = 0x01;
    pkt[idx++] = use_v2c ? 0x01 : 0x00;

    /* Community OCTET STRING */
    pkt[idx++] = ASN_OCTET_STR;
    pkt[idx++] = comm_len;
    memcpy(&pkt[idx], community, comm_len);
    idx += comm_len;

    /* GetRequest PDU */
    pkt[idx++] = ASN_GET_REQ;
    idx += asn_write_length(&pkt[idx], pdu_content_len);

    /* Request ID (INTEGER, 4 bytes) */
    pkt[idx++] = ASN_INTEGER;
    pkt[idx++] = 4;
    pkt[idx++] = (uint8_t)(request_id >> 24);
    pkt[idx++] = (uint8_t)(request_id >> 16);
    pkt[idx++] = (uint8_t)(request_id >> 8);
    pkt[idx++] = (uint8_t)(request_id);

    /* Error status = 0 */
    pkt[idx++] = ASN_INTEGER;
    pkt[idx++] = 0x01;
    pkt[idx++] = 0x00;

    /* Error index = 0 */
    pkt[idx++] = ASN_INTEGER;
    pkt[idx++] = 0x01;
    pkt[idx++] = 0x00;

    /* Varbind list SEQUENCE */
    pkt[idx++] = ASN_SEQUENCE;
    idx += asn_write_length(&pkt[idx], vb_seq_len);

    /* Varbind SEQUENCE */
    pkt[idx++] = ASN_SEQUENCE;
    idx += asn_write_length(&pkt[idx], varbind_len);

    /* OID */
    pkt[idx++] = ASN_OBJ_ID;
    pkt[idx++] = oid_len;
    memcpy(&pkt[idx], oid, oid_len);
    idx += oid_len;

    /* NULL value */
    pkt[idx++] = ASN_NULL;
    pkt[idx++] = 0x00;

    return idx;
}

/* Skip a TLV and return pointer past it; out_type/out_len filled */
static const uint8_t*
    asn_skip_tlv(const uint8_t* p, const uint8_t* end, uint8_t* out_type, uint16_t* out_len) {
    if(p >= end) return NULL;
    *out_type = *p++;
    if(p >= end) return NULL;
    uint16_t consumed = asn_read_length(p, out_len);
    p += consumed;
    return p;
}

/* Parse an INTEGER value (up to 4 bytes) */
static bool asn_parse_int(const uint8_t* val, uint16_t len, int32_t* out) {
    if(len == 0 || len > 4) return false;
    int32_t v = (val[0] & 0x80) ? -1 : 0; /* sign extension */
    for(uint16_t i = 0; i < len; i++) {
        v = (v << 8) | val[i];
    }
    *out = v;
    return true;
}

/* Parse unsigned integer (timeticks, counter, gauge) */
static bool asn_parse_uint(const uint8_t* val, uint16_t len, uint32_t* out) {
    if(len == 0 || len > 5) return false;
    uint32_t v = 0;
    for(uint16_t i = 0; i < len; i++) {
        v = (v << 8) | val[i];
    }
    *out = v;
    return true;
}

/* Check if response OID matches a known OID */
static bool
    oid_match(const uint8_t* resp_oid, uint16_t resp_len, const uint8_t* ref, uint8_t ref_len) {
    if(resp_len != ref_len) return false;
    return memcmp(resp_oid, ref, ref_len) == 0;
}

/**
 * Parse SNMP GET-Response and extract known OID values.
 */
static void snmp_parse_response(const uint8_t* buf, uint16_t len, SnmpGetResult* result) {
    const uint8_t* p = buf;
    const uint8_t* end = buf + len;
    uint8_t type;
    uint16_t tlen;

    /* Top-level SEQUENCE */
    p = asn_skip_tlv(p, end, &type, &tlen);
    if(!p || type != ASN_SEQUENCE) return;

    /* Version INTEGER - skip */
    const uint8_t* val = asn_skip_tlv(p, end, &type, &tlen);
    if(!val || type != ASN_INTEGER) return;
    p = val + tlen;

    /* Community - skip */
    val = asn_skip_tlv(p, end, &type, &tlen);
    if(!val || type != ASN_OCTET_STR) return;
    p = val + tlen;

    /* GetResponse PDU */
    val = asn_skip_tlv(p, end, &type, &tlen);
    if(!val || type != ASN_GET_RESP) return;
    p = val;

    /* Request ID - skip */
    val = asn_skip_tlv(p, end, &type, &tlen);
    if(!val) return;
    p = val + tlen;

    /* Error status */
    int32_t err_status = 0;
    val = asn_skip_tlv(p, end, &type, &tlen);
    if(!val) return;
    asn_parse_int(val, tlen, &err_status);
    p = val + tlen;
    if(err_status != 0) return;

    /* Error index - skip */
    val = asn_skip_tlv(p, end, &type, &tlen);
    if(!val) return;
    p = val + tlen;

    /* Varbind list SEQUENCE */
    val = asn_skip_tlv(p, end, &type, &tlen);
    if(!val || type != ASN_SEQUENCE) return;
    p = val;
    const uint8_t* vbl_end = val + tlen;

    /* Iterate varbinds */
    while(p < vbl_end && p < end) {
        /* Varbind SEQUENCE */
        val = asn_skip_tlv(p, end, &type, &tlen);
        if(!val || type != ASN_SEQUENCE) break;
        const uint8_t* vb_p = val;
        const uint8_t* vb_end = val + tlen;
        p = vb_end;

        /* OID */
        uint16_t oid_len;
        const uint8_t* oid_val = asn_skip_tlv(vb_p, vb_end, &type, &oid_len);
        if(!oid_val || type != ASN_OBJ_ID) continue;
        const uint8_t* oid_data = oid_val;
        vb_p = oid_val + oid_len;

        /* Value */
        uint16_t val_len;
        uint8_t val_type;
        const uint8_t* val_data = asn_skip_tlv(vb_p, vb_end, &val_type, &val_len);
        if(!val_data) continue;

        /* Match OIDs */
        if(oid_match(oid_data, oid_len, oid_sys_name, sizeof(oid_sys_name))) {
            if(val_type == ASN_OCTET_STR && val_len > 0) {
                uint16_t copy_len = val_len < SNMP_MAX_STRING - 1 ? val_len : SNMP_MAX_STRING - 1;
                memcpy(result->sys_name, val_data, copy_len);
                result->sys_name[copy_len] = '\0';
                result->has_sys_name = true;
            }
        } else if(oid_match(oid_data, oid_len, oid_sys_descr, sizeof(oid_sys_descr))) {
            if(val_type == ASN_OCTET_STR && val_len > 0) {
                uint16_t copy_len = val_len < SNMP_MAX_STRING - 1 ? val_len : SNMP_MAX_STRING - 1;
                memcpy(result->sys_descr, val_data, copy_len);
                result->sys_descr[copy_len] = '\0';
                result->has_sys_descr = true;
            }
        } else if(oid_match(oid_data, oid_len, oid_sys_uptime, sizeof(oid_sys_uptime))) {
            if(val_type == ASN_TIMETICKS || val_type == ASN_INTEGER) {
                asn_parse_uint(val_data, val_len, &result->sys_uptime);
                result->has_sys_uptime = true;
            }
        } else if(oid_match(oid_data, oid_len, oid_if_status, sizeof(oid_if_status))) {
            if(val_type == ASN_INTEGER) {
                asn_parse_int(val_data, val_len, &result->if_oper_status);
                result->has_if_status = true;
            }
        }
    }
}

/**
 * Send a single SNMP GET and receive response.
 */
static bool snmp_query_oid(
    const uint8_t target_ip[4],
    const char* community,
    bool use_v2c,
    uint32_t request_id,
    const uint8_t* oid,
    uint8_t oid_len,
    SnmpGetResult* result) {
    uint8_t* pkt = malloc(256);
    if(!pkt) return false;

    uint16_t pkt_len = snmp_build_get(pkt, 256, community, use_v2c, request_id, oid, oid_len);
    if(pkt_len == 0) {
        free(pkt);
        return false;
    }

    close(SNMP_SOCK);
    if(socket(SNMP_SOCK, Sn_MR_UDP, SNMP_LOCAL_PORT, 0) != SNMP_SOCK) {
        free(pkt);
        return false;
    }

    if(sendto(SNMP_SOCK, pkt, pkt_len, (uint8_t*)target_ip, SNMP_PORT) <= 0) {
        close(SNMP_SOCK);
        free(pkt);
        return false;
    }

    uint32_t start = furi_get_tick();
    bool got_reply = false;

    while((furi_get_tick() - start) < SNMP_TIMEOUT_MS) {
        uint16_t rx_len = getSn_RX_RSR(SNMP_SOCK);
        if(rx_len > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t recv_len = recvfrom(SNMP_SOCK, pkt, 256, from_ip, &from_port);
            if(recv_len > 0) {
                snmp_parse_response(pkt, (uint16_t)recv_len, result);
                got_reply = true;
                break;
            }
        }
        furi_delay_ms(10);
    }

    close(SNMP_SOCK);
    free(pkt);
    return got_reply;
}

bool snmp_client_get(
    const uint8_t target_ip[4],
    const char* community,
    bool use_v2c,
    SnmpGetResult* result) {
    memset(result, 0, sizeof(SnmpGetResult));

    /* Query each OID separately for reliability */
    uint32_t rid = 1;

    snmp_query_oid(
        target_ip, community, use_v2c, rid++, oid_sys_name, sizeof(oid_sys_name), result);

    snmp_query_oid(
        target_ip, community, use_v2c, rid++, oid_sys_descr, sizeof(oid_sys_descr), result);

    snmp_query_oid(
        target_ip, community, use_v2c, rid++, oid_sys_uptime, sizeof(oid_sys_uptime), result);

    snmp_query_oid(
        target_ip, community, use_v2c, rid++, oid_if_status, sizeof(oid_if_status), result);

    result->valid = result->has_sys_name || result->has_sys_descr || result->has_sys_uptime ||
                    result->has_if_status;
    return result->valid;
}
