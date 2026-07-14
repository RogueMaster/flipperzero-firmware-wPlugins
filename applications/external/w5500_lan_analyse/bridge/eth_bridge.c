#include "eth_bridge.h"
#include "../usb_eth/usb_eth.h"
#include "../hal/w5500_hal.h"

#include <furi.h>
#include <string.h>

#define TAG "ETH_BRIDGE"

void eth_bridge_init(EthBridgeState* state) {
    memset(state, 0, sizeof(EthBridgeState));
    state->running = true;
}

void eth_bridge_poll(EthBridgeState* state, uint8_t* frame_buf, uint16_t buf_size) {
    int16_t len;

    /* Update connection states */
    state->usb_connected = usb_eth_is_connected();
    state->lan_link_up = w5500_hal_get_link_status();

    /* USB -> Ethernet (host -> LAN) */
    len = usb_eth_receive_frame(frame_buf, buf_size);
    if(len > 0) {
        uint16_t sent = w5500_hal_macraw_send(frame_buf, (uint16_t)len);
        if(sent > 0) {
            state->frames_usb_to_eth++;
            state->bytes_usb_to_eth += (uint32_t)len;
            if(state->dump_enabled && state->pcap.active) {
                pcap_dump_frame(&state->pcap, frame_buf, (uint16_t)len);
            }
        } else {
            state->errors++;
        }
    }

    /* Ethernet -> USB (LAN -> host) */
    uint16_t recv_len = w5500_hal_macraw_recv(frame_buf, buf_size);
    if(recv_len > 0) {
        if(usb_eth_send_frame(frame_buf, recv_len)) {
            state->frames_eth_to_usb++;
            state->bytes_eth_to_usb += recv_len;
            if(state->dump_enabled && state->pcap.active) {
                pcap_dump_frame(&state->pcap, frame_buf, recv_len);
            }
        } else {
            state->errors++;
        }
    }
}
