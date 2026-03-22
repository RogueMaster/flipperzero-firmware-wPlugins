#include "wfr_encode.h"
#include <infrared_transmit.h>
#include <furi.h>

/* Send one RC-6 message */
static void wfr_send_rc6(uint8_t address, uint8_t command) {
    InfraredMessage message = {
        .protocol = InfraredProtocolRC6,
        .address = address,
        .command = command,
        .repeat = false,
    };
    infrared_send(&message, 1);
}

bool wfr_transmit_credentials(const WfrWifiCreds* creds) {
    if(!creds || creds->ssid[0] == '\0') return false;

    /* Build WiFi QR string */
    char wifi_str[WFR_MAX_TOTAL_PAYLOAD + 1];
    size_t wifi_len = wfr_build_wifi_string(creds, wifi_str, sizeof(wifi_str));
    if(wifi_len == 0 || wifi_len > 255) return false;

    const uint8_t* payload = (const uint8_t*)wifi_str;
    uint8_t payload_crc = wfr_crc8(payload, wifi_len);

    /* Retransmit the full sequence multiple times */
    for(uint8_t attempt = 0; attempt < WFR_RETRANSMIT_COUNT; attempt++) {
        uint8_t pass = attempt & WFR_RC6_PASS_MASK;

        /* --- START: address = magic | TYPE_START | pass, command = length --- */
        wfr_send_rc6(WFR_RC6_MAGIC | WFR_RC6_TYPE_START | pass, (uint8_t)wifi_len);
        furi_delay_ms(WFR_RC6_INTER_MSG_MS);

        /* --- DATA: one RC-6 message per payload byte --- */
        for(size_t i = 0; i < wifi_len; i++) {
            wfr_send_rc6(WFR_RC6_MAGIC | WFR_RC6_TYPE_DATA | pass, payload[i]);
            furi_delay_ms(WFR_RC6_INTER_MSG_MS);
        }

        /* --- END: address = magic | TYPE_END | pass, command = CRC-8 --- */
        wfr_send_rc6(WFR_RC6_MAGIC | WFR_RC6_TYPE_END | pass, payload_crc);

        /* Gap between retransmission passes */
        if(attempt + 1 < WFR_RETRANSMIT_COUNT) {
            furi_delay_ms(WFR_RC6_RETRANSMIT_GAP_MS);
        }
    }

    return true;
}
