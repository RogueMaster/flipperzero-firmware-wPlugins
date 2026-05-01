#include "eapol_probe.h"
#include "../utils/packet_utils.h"
#include "../hal/w5500_hal.h"

#include <furi.h>
#include <string.h>

/* EAPOL constants */
#define ETH_TYPE_EAPOL 0x888E
#define EAPOL_VERSION  1
#define EAPOL_START    1
#define EAPOL_PACKET   0

/* EAP codes */
#define EAP_REQUEST  1
#define EAP_RESPONSE 2
#define EAP_SUCCESS  3
#define EAP_FAILURE  4

/* 802.1X multicast PAE group address */
static const uint8_t pae_group_addr[6] = {0x01, 0x80, 0xC2, 0x00, 0x00, 0x03};

/**
 * Build EAPOL-Start frame.
 * Returns frame length.
 */
static uint16_t eapol_build_start(uint8_t* frame, const uint8_t our_mac[6]) {
    /* Destination: PAE group address */
    memcpy(&frame[0], pae_group_addr, 6);
    /* Source: our MAC */
    memcpy(&frame[6], our_mac, 6);
    /* EtherType: 802.1X */
    pkt_write_u16_be(&frame[12], ETH_TYPE_EAPOL);

    /* EAPOL header (4 bytes) */
    frame[14] = EAPOL_VERSION; /* Protocol Version */
    frame[15] = EAPOL_START; /* Type: EAPOL-Start */
    frame[16] = 0; /* Body Length: 0 (MSB) */
    frame[17] = 0; /* Body Length: 0 (LSB) */

    return 18;
}

/**
 * Parse EAPOL frame from raw Ethernet.
 */
static void eapol_parse_frame(const uint8_t* frame, uint16_t len, EapolProbeResult* result) {
    if(len < 18) return;

    uint16_t ethertype = pkt_get_ethertype(frame);
    if(ethertype != ETH_TYPE_EAPOL) return;

    result->frames_seen++;
    result->eapol_response = true;

    /* Save authenticator MAC */
    pkt_get_src_mac(frame, result->auth_mac);

    /* EAPOL header at offset 14 */
    uint8_t eapol_type = frame[15];
    uint16_t body_len = pkt_read_u16_be(&frame[16]);

    if(eapol_type == EAPOL_PACKET && body_len >= 4 && len >= 22) {
        /* EAP packet: code(1) + id(1) + length(2) + [type(1)] */
        uint8_t eap_code = frame[18];

        switch(eap_code) {
        case EAP_REQUEST:
            result->eap_request = true;
            if(body_len >= 5 && len >= 23) {
                result->eap_type = frame[22]; /* EAP type */
            }
            break;
        case EAP_SUCCESS:
            result->eap_success = true;
            break;
        case EAP_FAILURE:
            result->eap_failure = true;
            break;
        }
    }
}

bool eapol_probe_test(const uint8_t our_mac[6], EapolProbeResult* result) {
    memset(result, 0, sizeof(EapolProbeResult));

    if(!w5500_hal_open_macraw()) return false;

    /* Build and send EAPOL-Start */
    uint8_t frame[64];
    uint16_t frame_len = eapol_build_start(frame, our_mac);
    w5500_hal_macraw_send(frame, frame_len);

    /* Listen for response (up to 5 seconds) */
    uint32_t start = furi_get_tick();
    uint32_t timeout_ms = 5000;
    /* Static to avoid 256B stack usage; worker is single-threaded */
    static uint8_t rx_buf[256];

    /* Send EAPOL-Start 3 times with 1-second intervals for reliability */
    uint8_t sends = 1;

    while((furi_get_tick() - start) < timeout_ms) {
        uint16_t recv_len = w5500_hal_macraw_recv(rx_buf, sizeof(rx_buf));
        if(recv_len > 0) {
            eapol_parse_frame(rx_buf, recv_len, result);
            if(result->eap_request || result->eap_success || result->eap_failure) {
                break; /* Got a definitive response */
            }
        }

        /* Resend EAPOL-Start every second */
        if(sends < 3 && (furi_get_tick() - start) > sends * 1000) {
            w5500_hal_macraw_send(frame, frame_len);
            sends++;
        }

        furi_delay_ms(5);
    }

    w5500_hal_close_macraw();

    result->valid = true;
    return result->eapol_response;
}
