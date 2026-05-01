#include "traceroute.h"

#include <furi.h>
#include <furi_hal.h>
#include <socket.h>
#include <w5500.h>
#include <wizchip_conf.h>
#include <string.h>

#include "../utils/packet_utils.h"

#define TAG "TRACERT"

/* ICMP packet constants */
#define ICMP_HEADER_SIZE 8
#define ICMP_DATA_SIZE   32
#define ICMP_PACKET_SIZE (ICMP_HEADER_SIZE + ICMP_DATA_SIZE)
#define IP_PROTO_ICMP    1

bool traceroute_send_hop(
    uint8_t socket_num,
    const uint8_t target_ip[4],
    uint8_t ttl,
    uint16_t seq,
    uint32_t timeout_ms,
    TracerouteHop* hop) {
    furi_assert(hop);
    memset(hop, 0, sizeof(TracerouteHop));
    hop->ttl = ttl;

    /* Close any previous socket state */
    close(socket_num);

    /* Set IP protocol to ICMP */
    WIZCHIP_WRITE(Sn_PROTO(socket_num), IP_PROTO_ICMP);

    /* Set TTL for this socket */
    WIZCHIP_WRITE(Sn_TTL(socket_num), ttl);

    /* Open IPRAW socket */
    int8_t ret = socket(socket_num, Sn_MR_IPRAW, IP_PROTO_ICMP, 0);
    if(ret != socket_num) {
        FURI_LOG_E(TAG, "Failed to open IPRAW socket: %d", ret);
        return false;
    }

    /* Build ICMP Echo Request */
    uint8_t icmp_buf[ICMP_PACKET_SIZE];
    memset(icmp_buf, 0, sizeof(icmp_buf));
    icmp_buf[0] = 8; /* Echo Request */
    icmp_buf[1] = 0; /* Code */
    icmp_buf[4] = 0x00; /* Identifier high */
    icmp_buf[5] = 0x01; /* Identifier low */
    icmp_buf[6] = (uint8_t)(seq >> 8);
    icmp_buf[7] = (uint8_t)(seq & 0xFF);

    for(uint8_t i = 0; i < ICMP_DATA_SIZE; i++) {
        icmp_buf[ICMP_HEADER_SIZE + i] = (uint8_t)(i + 0x30);
    }

    uint16_t cksum = pkt_checksum(icmp_buf, ICMP_PACKET_SIZE);
    icmp_buf[2] = (uint8_t)(cksum >> 8);
    icmp_buf[3] = (uint8_t)(cksum & 0xFF);

    /* Send */
    int32_t sent = sendto(socket_num, icmp_buf, ICMP_PACKET_SIZE, (uint8_t*)target_ip, 1);
    if(sent <= 0) {
        FURI_LOG_E(TAG, "Failed to send ICMP: %ld", sent);
        close(socket_num);
        return false;
    }

    /* Wait for response (Time Exceeded or Echo Reply) */
    uint32_t start_tick = furi_get_tick();
    /* Static to avoid 128B stack usage; worker is single-threaded */
    static uint8_t recv_buf[128];
    uint8_t from_ip[4];
    uint16_t from_port;

    while(furi_get_tick() - start_tick < timeout_ms) {
        uint16_t rx_size = getSn_RX_RSR(socket_num);
        if(rx_size > 0) {
            int32_t received =
                recvfrom(socket_num, recv_buf, sizeof(recv_buf), from_ip, &from_port);
            if(received >= 1) {
                uint8_t icmp_type = recv_buf[0];

                if(icmp_type == ICMP_TIME_EXCEEDED) {
                    /* Intermediate router */
                    memcpy(hop->hop_ip, from_ip, 4);
                    hop->rtt_ms = furi_get_tick() - start_tick;
                    hop->responded = true;
                    hop->is_destination = false;
                    close(socket_num);
                    return true;
                }

                if(icmp_type == ICMP_ECHO_REPLY) {
                    /* Destination reached */
                    memcpy(hop->hop_ip, from_ip, 4);
                    hop->rtt_ms = furi_get_tick() - start_tick;
                    hop->responded = true;
                    hop->is_destination = true;
                    close(socket_num);
                    return true;
                }
            }
        }
        furi_delay_ms(5);
    }

    /* Timeout */
    close(socket_num);
    return false;
}
