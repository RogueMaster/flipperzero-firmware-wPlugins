#include "ntp_diag.h"
#include <furi.h>
#include <socket.h>
#include <string.h>
#include <stdio.h>

#define NTP_SOCK       3
#define NTP_PORT       123
#define NTP_LOCAL_PORT 12300
#define NTP_TIMEOUT_MS 3000
#define NTP_PKT_SIZE   48

/* NTP timestamp: seconds since 1900-01-01 (32-bit) + fraction (32-bit) */
typedef struct {
    uint32_t seconds;
    uint32_t fraction;
} NtpTimestamp;

static uint32_t read_u32_be(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void write_u32_be(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

/* Stratum description */
static void stratum_to_name(uint8_t stratum, char* buf, uint8_t buf_size) {
    if(stratum == 0) {
        snprintf(buf, buf_size, "Kiss-o'-Death");
    } else if(stratum == 1) {
        snprintf(buf, buf_size, "Primary (atomic/GPS)");
    } else if(stratum <= 15) {
        snprintf(buf, buf_size, "Secondary (lvl %d)", stratum);
    } else {
        snprintf(buf, buf_size, "Unsynchronized");
    }
}

/* For stratum 1, ref_id is 4 ASCII chars (e.g. "GPS\0", "PPS\0") */
static void refid_to_str(uint32_t ref_id, uint8_t stratum, char* buf) {
    if(stratum <= 1) {
        buf[0] = (char)((ref_id >> 24) & 0xFF);
        buf[1] = (char)((ref_id >> 16) & 0xFF);
        buf[2] = (char)((ref_id >> 8) & 0xFF);
        buf[3] = (char)(ref_id & 0xFF);
        buf[4] = '\0';
        /* Clean non-printable */
        for(int i = 0; i < 4; i++) {
            if(buf[i] < 0x20 || buf[i] > 0x7E) buf[i] = '\0';
        }
    } else {
        /* Stratum 2+: ref_id is IP of reference clock */
        snprintf(
            buf,
            16,
            "%u.%u.%u.%u",
            (unsigned)((ref_id >> 24) & 0xFF),
            (unsigned)((ref_id >> 16) & 0xFF),
            (unsigned)((ref_id >> 8) & 0xFF),
            (unsigned)(ref_id & 0xFF));
    }
}

bool ntp_diag_query(const uint8_t server_ip[4], NtpDiagResult* result) {
    memset(result, 0, sizeof(NtpDiagResult));

    close(NTP_SOCK);
    if(socket(NTP_SOCK, Sn_MR_UDP, NTP_LOCAL_PORT, 0) != NTP_SOCK) return false;

    /* Build NTP client request (mode 3, version 4) */
    uint8_t pkt[NTP_PKT_SIZE];
    memset(pkt, 0, sizeof(pkt));
    pkt[0] = (0 << 6) | (4 << 3) | 3; /* LI=0, VN=4, Mode=3 (client) */

    /* Record T1 (transmit timestamp from client perspective) */
    uint32_t t1_tick = furi_get_tick();

    /* Write a simple transmit timestamp for correlation */
    write_u32_be(&pkt[40], t1_tick); /* xmt seconds (arbitrary, for matching) */

    if(sendto(NTP_SOCK, pkt, NTP_PKT_SIZE, (uint8_t*)server_ip, NTP_PORT) <= 0) {
        close(NTP_SOCK);
        return false;
    }

    /* Wait for response */
    uint32_t start = furi_get_tick();
    bool got_reply = false;

    while((furi_get_tick() - start) < NTP_TIMEOUT_MS) {
        uint16_t rx_len = getSn_RX_RSR(NTP_SOCK);
        if(rx_len > 0) {
            uint8_t from_ip[4];
            uint16_t from_port;
            int32_t recv_len = recvfrom(NTP_SOCK, pkt, sizeof(pkt), from_ip, &from_port);
            if(recv_len >= NTP_PKT_SIZE) {
                uint32_t t4_tick = furi_get_tick();

                /* Parse NTP response */
                result->leap = (pkt[0] >> 6) & 0x03;
                result->version = (pkt[0] >> 3) & 0x07;
                result->mode = pkt[0] & 0x07;
                result->stratum = pkt[1];
                result->poll = (int8_t)pkt[2];
                result->precision = (int8_t)pkt[3];
                result->root_delay = read_u32_be(&pkt[4]);
                result->root_disp = read_u32_be(&pkt[8]);
                result->ref_id = read_u32_be(&pkt[12]);

                /* RTT in microseconds (approximate from tick ms) */
                uint32_t rtt_ms = t4_tick - t1_tick;
                result->rtt_us = rtt_ms * 1000;

                /* Offset: simplified estimate = RTT/2 (we don't have real NTP timestamps) */
                result->offset_us = (int32_t)(rtt_ms * 500); /* half RTT as rough estimate */

                stratum_to_name(
                    result->stratum, result->stratum_name, sizeof(result->stratum_name));
                refid_to_str(result->ref_id, result->stratum, result->ref_id_str);

                result->valid = true;
                got_reply = true;
                break;
            }
        }
        furi_delay_ms(10);
    }

    close(NTP_SOCK);
    return got_reply;
}
