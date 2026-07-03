#include "icmp.h"

#include <furi.h>
#include <furi_hal.h>
#include <socket.h>
#include <wizchip_conf.h>
#include <string.h>

#include "../utils/packet_utils.h"

#define TAG "ICMP"

/* ICMP packet structure */
#define ICMP_HEADER_SIZE 8
#define ICMP_DATA_SIZE   32
#define ICMP_PACKET_SIZE (ICMP_HEADER_SIZE + ICMP_DATA_SIZE)

/* IPRAW protocol number for ICMP */
#define IP_PROTO_ICMP 1

bool icmp_ping(
    uint8_t socket_num,
    const uint8_t target_ip[4],
    uint16_t seq,
    uint32_t timeout_ms,
    PingResult* result,
    const volatile bool* running) {
    furi_assert(result);

    memcpy(result->target_ip, target_ip, 4);
    result->seq = seq;
    result->rtt_ms = 0;
    result->success = false;

    /* Close any previous socket state */
    close(socket_num);

    /* Set IP protocol register to ICMP before opening IPRAW socket.
     * W5500 header defines Sn_PROTO address but not setSn_PROTO macro. */
    WIZCHIP_WRITE(Sn_PROTO(socket_num), IP_PROTO_ICMP);

    /* Open socket in IPRAW mode for ICMP */
    int8_t ret = socket(socket_num, Sn_MR_IPRAW, IP_PROTO_ICMP, 0);
    if(ret != socket_num) {
        FURI_LOG_E(TAG, "Failed to open IPRAW socket: %d", ret);
        return false;
    }

    /* Build ICMP Echo Request */
    uint8_t icmp_buf[ICMP_PACKET_SIZE];
    memset(icmp_buf, 0, sizeof(icmp_buf));

    icmp_buf[0] = ICMP_TYPE_ECHO_REQUEST; /* Type */
    icmp_buf[1] = 0; /* Code */
    /* Checksum at [2-3], fill later */
    icmp_buf[4] = 0x00; /* Identifier high */
    icmp_buf[5] = 0x01; /* Identifier low */
    icmp_buf[6] = (uint8_t)(seq >> 8); /* Sequence high */
    icmp_buf[7] = (uint8_t)(seq & 0xFF); /* Sequence low */

    /* Fill data with pattern */
    for(uint8_t i = 0; i < ICMP_DATA_SIZE; i++) {
        icmp_buf[ICMP_HEADER_SIZE + i] = (uint8_t)(i + 0x30);
    }

    /* Calculate checksum */
    uint16_t cksum = pkt_checksum(icmp_buf, ICMP_PACKET_SIZE);
    icmp_buf[2] = (uint8_t)(cksum >> 8);
    icmp_buf[3] = (uint8_t)(cksum & 0xFF);

    /* Send ICMP request (port must be non-zero for WIZnet sendto validation) */
    int32_t sent = sendto(socket_num, icmp_buf, ICMP_PACKET_SIZE, (uint8_t*)target_ip, 1);
    if(sent <= 0) {
        FURI_LOG_E(TAG, "Failed to send ICMP request: %ld", sent);
        close(socket_num);
        return false;
    }

    /* Wait for reply */
    uint32_t start_tick = furi_get_tick();
    uint8_t recv_buf[ICMP_PACKET_SIZE + 20]; /* Extra space for any header data */
    uint8_t from_ip[4];
    uint16_t from_port;

    while(furi_get_tick() - start_tick < timeout_ms) {
        if(running && !*running) {
            close(socket_num);
            return false;
        }
        uint16_t rx_size = getSn_RX_RSR(socket_num);
        if(rx_size > 0) {
            int32_t received =
                recvfrom(socket_num, recv_buf, sizeof(recv_buf), from_ip, &from_port);
            if(received >= ICMP_HEADER_SIZE) {
                /* Check if this is an Echo Reply */
                if(recv_buf[0] == ICMP_TYPE_ECHO_REPLY) {
                    uint16_t recv_seq = ((uint16_t)recv_buf[6] << 8) | recv_buf[7];
                    if(recv_seq == seq) {
                        result->rtt_ms = furi_get_tick() - start_tick;
                        result->success = true;
                        close(socket_num);
                        return true;
                    }
                }
            }
        }
        furi_delay_ms(1);
    }

    FURI_LOG_W(
        TAG,
        "Ping timeout for %d.%d.%d.%d",
        target_ip[0],
        target_ip[1],
        target_ip[2],
        target_ip[3]);
    close(socket_num);
    return false;
}
