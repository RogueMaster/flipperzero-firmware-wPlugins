#include "ipmi_client.h"

#include <furi.h>
#include <socket.h>
#include <string.h>

#define IPMI_SOCK       3
#define IPMI_PORT       623
#define IPMI_LOCAL_PORT 16230
#define IPMI_TIMEOUT_MS 3000

/* RMCP header */
#define RMCP_VERSION    0x06
#define RMCP_SEQ_NO     0xFF /* no RMCP ACK */
#define RMCP_CLASS_IPMI 0x07

/* IPMI v1.5 session header (unauthenticated) */
#define IPMI_AUTH_NONE 0x00
#define IPMI_SEQ_NONE  0x00000000
#define IPMI_SID_NONE  0x00000000

/* IPMI commands */
#define IPMI_NETFN_CHASSIS          0x00
#define IPMI_NETFN_APP              0x06
#define IPMI_CMD_GET_CHASSIS_STATUS 0x01
#define IPMI_CMD_GET_DEVICE_ID      0x01

static void write_u32_le(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/**
 * Calculate IPMI checksum (two's complement of sum).
 */
static uint8_t ipmi_checksum(const uint8_t* data, uint8_t len) {
    uint8_t sum = 0;
    for(uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)(~sum + 1);
}

/**
 * Build IPMI v1.5 over LAN message (unauthenticated).
 *
 * Format: RMCP(4) + Session(10) + IPMI Message (variable)
 */
static uint16_t
    ipmi_build_request(uint8_t* pkt, uint16_t pkt_size, uint8_t netfn, uint8_t cmd, uint8_t rq_seq) {
    if(pkt_size < 30) return 0;
    uint16_t idx = 0;

    /* RMCP Header (4 bytes) */
    pkt[idx++] = RMCP_VERSION;
    pkt[idx++] = 0x00; /* reserved */
    pkt[idx++] = RMCP_SEQ_NO;
    pkt[idx++] = RMCP_CLASS_IPMI;

    /* IPMI v1.5 Session Header (10 bytes) */
    pkt[idx++] = IPMI_AUTH_NONE; /* auth type = none */
    write_u32_le(&pkt[idx], IPMI_SEQ_NONE); /* session seq */
    idx += 4;
    write_u32_le(&pkt[idx], IPMI_SID_NONE); /* session ID */
    idx += 4;

    /* Message length (will be filled after building IPMI msg) */
    uint16_t msg_len_idx = idx;
    pkt[idx++] = 0; /* placeholder */

    /* IPMI Message */
    uint16_t msg_start = idx;

    /* rsAddr = 0x20 (BMC), netFn/rsLUN */
    pkt[idx++] = 0x20; /* rsAddr: BMC */
    pkt[idx++] = (netfn << 2) | 0x00; /* netFn/rsLUN */

    /* Checksum 1: over rsAddr + netFn/rsLUN */
    pkt[idx] = ipmi_checksum(&pkt[msg_start], 2);
    idx++;

    /* rqAddr, rqSeq/rqLUN, cmd */
    pkt[idx++] = 0x81; /* rqAddr: remote console */
    pkt[idx++] = (rq_seq << 2) | 0x00; /* rqSeq/rqLUN */
    pkt[idx++] = cmd;

    /* Checksum 2: over rqAddr + rqSeq + cmd */
    pkt[idx] = ipmi_checksum(&pkt[msg_start + 3], 3);
    idx++;

    /* Fill message length */
    pkt[msg_len_idx] = (uint8_t)(idx - msg_start);

    return idx;
}

/**
 * Parse IPMI response.
 */
static bool ipmi_parse_response(
    const uint8_t* pkt,
    uint16_t len,
    uint8_t expected_netfn,
    uint8_t expected_cmd __attribute__((unused)),
    const uint8_t** data_out,
    uint8_t* data_len_out) {
    /* Minimum: RMCP(4) + Session(10) + msglen(1) + IPMI(7) = 22 */
    if(len < 22) return false;

    /* Check RMCP */
    if(pkt[0] != RMCP_VERSION) return false;
    if(pkt[3] != RMCP_CLASS_IPMI) return false;

    /* Skip session header (10 bytes from offset 4) */
    uint16_t msg_offset = 14;
    uint8_t msg_len = pkt[msg_offset];
    msg_offset++;

    if(msg_offset + msg_len > len) return false;
    if(msg_len < 7) return false;

    /* Check response netFn (should be request netFn | 0x01 for response) */
    uint8_t resp_netfn = (pkt[msg_offset + 1] >> 2) & 0x3F;
    if(resp_netfn != (expected_netfn | 0x01)) return false;

    /* Completion code at offset +6 */
    uint8_t cc = pkt[msg_offset + 6];
    if(cc != 0x00) return false; /* non-zero = error */

    /* Data starts after completion code */
    *data_out = &pkt[msg_offset + 7];
    *data_len_out = msg_len - 8; /* minus header(6) + cc(1) + checksum(1) */

    return true;
}

/**
 * Send IPMI command and receive response.
 */
static bool ipmi_send_recv(
    const uint8_t target_ip[4],
    uint8_t netfn,
    uint8_t cmd,
    uint8_t rq_seq,
    const uint8_t** resp_data,
    uint8_t* resp_len) {
    /* Static to avoid 128B stack usage; worker is single-threaded */
    static uint8_t pkt[128];
    uint16_t pkt_len = ipmi_build_request(pkt, sizeof(pkt), netfn, cmd, rq_seq);
    if(pkt_len == 0) return false;

    close(IPMI_SOCK);
    if(socket(IPMI_SOCK, Sn_MR_UDP, IPMI_LOCAL_PORT + rq_seq, 0) != IPMI_SOCK) return false;

    if(sendto(IPMI_SOCK, pkt, pkt_len, (uint8_t*)target_ip, IPMI_PORT) <= 0) {
        close(IPMI_SOCK);
        return false;
    }

    uint32_t start = furi_get_tick();
    bool ok = false;
    static uint8_t rx_buf[128];

    while((furi_get_tick() - start) < IPMI_TIMEOUT_MS) {
        uint16_t rx_len = getSn_RX_RSR(IPMI_SOCK);
        if(rx_len > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t recv_len = recvfrom(IPMI_SOCK, rx_buf, sizeof(rx_buf), from_ip, &from_port);
            if(recv_len > 0) {
                if(ipmi_parse_response(
                       rx_buf, (uint16_t)recv_len, netfn, cmd, resp_data, resp_len)) {
                    ok = true;
                    break;
                }
            }
        }
        furi_delay_ms(10);
    }

    close(IPMI_SOCK);
    return ok;
}

bool ipmi_query(const uint8_t target_ip[4], IpmiResult* result) {
    memset(result, 0, sizeof(IpmiResult));

    /* Get Chassis Status */
    const uint8_t* data;
    uint8_t data_len;

    if(ipmi_send_recv(
           target_ip, IPMI_NETFN_CHASSIS, IPMI_CMD_GET_CHASSIS_STATUS, 1, &data, &data_len)) {
        if(data_len >= 3) {
            result->power_state = data[0];
            result->chassis_ok = true;
        }
    }

    furi_delay_ms(200);

    /* Get Device ID */
    if(ipmi_send_recv(target_ip, IPMI_NETFN_APP, IPMI_CMD_GET_DEVICE_ID, 2, &data, &data_len)) {
        if(data_len >= 5) {
            result->device_id = data[0];
            result->device_revision = data[1] & 0x0F;
            result->firmware_major = data[2] & 0x7F;
            result->firmware_minor = data[3];
            result->ipmi_version = data[4];
            result->device_ok = true;
        }
    }

    result->valid = result->chassis_ok || result->device_ok;
    if(!result->valid) {
        strncpy(result->error_msg, "No IPMI response", sizeof(result->error_msg));
    }

    return result->valid;
}
