#include "wol.h"

#include <furi.h>
#include <socket.h>
#include <wizchip_conf.h>
#include <string.h>

#define TAG "WOL"

uint16_t wol_build_magic_packet(uint8_t* buf, const uint8_t target_mac[6]) {
    /* 6 bytes of 0xFF */
    memset(buf, 0xFF, 6);

    /* 16 repetitions of target MAC */
    for(uint8_t i = 0; i < 16; i++) {
        memcpy(&buf[6 + i * 6], target_mac, 6);
    }

    return WOL_PACKET_SIZE;
}

bool wol_send(uint8_t socket_num, const uint8_t target_mac[6]) {
    /* Build magic packet */
    uint8_t pkt[WOL_PACKET_SIZE];
    wol_build_magic_packet(pkt, target_mac);

    /* Open UDP socket */
    close(socket_num);
    int8_t ret = socket(socket_num, Sn_MR_UDP, 0, 0);
    if(ret != socket_num) {
        FURI_LOG_E(TAG, "Failed to open UDP socket: %d", ret);
        return false;
    }

    /* Send to broadcast 255.255.255.255:9 */
    uint8_t bcast_ip[4] = {255, 255, 255, 255};
    int32_t sent = sendto(socket_num, pkt, WOL_PACKET_SIZE, bcast_ip, WOL_PORT);

    close(socket_num);

    if(sent <= 0) {
        FURI_LOG_E(TAG, "Failed to send WoL packet: %ld", sent);
        return false;
    }

    FURI_LOG_I(
        TAG,
        "WoL sent to %02X:%02X:%02X:%02X:%02X:%02X",
        target_mac[0],
        target_mac[1],
        target_mac[2],
        target_mac[3],
        target_mac[4],
        target_mac[5]);

    return true;
}
