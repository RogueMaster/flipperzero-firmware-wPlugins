#include "wfr_encode.h"
#include <infrared_transmit.h>
#include <furi.h>

/* Send one IR message using the selected protocol */
static void wfr_send_ir(WfrIrProtocol protocol, uint8_t address, uint8_t command) {
    InfraredMessage message = {
        .protocol = (protocol == WfrIrProtocolNEC) ? InfraredProtocolNEC : InfraredProtocolRC6,
        .address = address,
        .command = command,
        .repeat = false,
    };
    infrared_send(&message, 1);
}

bool wfr_transmit_credentials(const WfrWifiCreds* creds, WfrIrProtocol protocol) {
    if(!creds || creds->ssid[0] == '\0') return false;

    /* Build WiFi QR string */
    char wifi_str[WFR_MAX_TOTAL_PAYLOAD + 1];
    size_t wifi_len = wfr_build_wifi_string(creds, wifi_str, sizeof(wifi_str));
    if(wifi_len == 0 || wifi_len > 255) return false;

    const uint8_t* payload = (const uint8_t*)wifi_str;
    uint8_t payload_crc = wfr_crc8(payload, wifi_len);

    /* Select timing based on protocol */
    uint32_t inter_msg_ms = (protocol == WfrIrProtocolNEC) ? WFR_NEC_INTER_MSG_MS :
                                                             WFR_RC6_INTER_MSG_MS;
    uint32_t retransmit_gap_ms = (protocol == WfrIrProtocolNEC) ? WFR_NEC_RETRANSMIT_GAP_MS :
                                                                  WFR_RC6_RETRANSMIT_GAP_MS;

    /* Retransmit the full sequence multiple times */
    for(uint8_t attempt = 0; attempt < WFR_RETRANSMIT_COUNT; attempt++) {
        uint8_t pass = attempt & WFR_FRAME_PASS_MASK;

        /* --- START: address = magic | TYPE_START | pass, command = length --- */
        wfr_send_ir(protocol, WFR_FRAME_MAGIC | WFR_FRAME_TYPE_START | pass, (uint8_t)wifi_len);
        furi_delay_ms(inter_msg_ms);

        /* --- DATA: one IR message per payload byte --- */
        for(size_t i = 0; i < wifi_len; i++) {
            wfr_send_ir(protocol, WFR_FRAME_MAGIC | WFR_FRAME_TYPE_DATA | pass, payload[i]);
            furi_delay_ms(inter_msg_ms);
        }

        /* --- END: address = magic | TYPE_END | pass, command = CRC-8 --- */
        wfr_send_ir(protocol, WFR_FRAME_MAGIC | WFR_FRAME_TYPE_END | pass, payload_crc);

        /* Gap between retransmission passes */
        if(attempt + 1 < WFR_RETRANSMIT_COUNT) {
            furi_delay_ms(retransmit_gap_ms);
        }
    }

    return true;
}
