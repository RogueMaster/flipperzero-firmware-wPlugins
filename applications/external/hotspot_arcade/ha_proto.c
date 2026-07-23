#include "ha_proto.h"

void ha_proto_send(HaUart* uart, uint8_t type, const uint8_t* payload, size_t len) {
    if(len > HA_MAX_PAYLOAD) len = HA_MAX_PAYLOAD;
    uint8_t hdr[4] = {HA_SYNC, type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    uint8_t crc = ha_crc8_upd(0, type);
    crc = ha_crc8_upd(crc, hdr[2]);
    crc = ha_crc8_upd(crc, hdr[3]);
    for(size_t i = 0; i < len; i++)
        crc = ha_crc8_upd(crc, payload[i]);

    ha_uart_tx(uart, hdr, 4);
    if(len) ha_uart_tx(uart, payload, len);
    ha_uart_tx(uart, &crc, 1);
}

void ha_proto_send_str(HaUart* uart, uint8_t type, const char* s) {
    ha_proto_send(uart, type, (const uint8_t*)s, strlen(s));
}
