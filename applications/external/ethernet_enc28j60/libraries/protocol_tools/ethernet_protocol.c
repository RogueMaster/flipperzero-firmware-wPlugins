#include "ethernet_protocol.h"

bool set_ethernet_header(
    uint8_t* buffer,
    uint8_t* mac_origin,
    uint8_t* mac_destination,
    uint16_t type) {
    if(!buffer || !mac_origin || !mac_destination) {
        return false;
    }

    ethernet_header_t* ethernet_header = (ethernet_header_t*)buffer;

    memcpy(ethernet_header->mac_destination, mac_destination, 6);
    memcpy(ethernet_header->mac_source, mac_origin, 6);

    ethernet_header->type[0] = (type >> 8) & 0xff;
    ethernet_header->type[1] = type & 0xff;

    return true;
}

ethernet_header_t ethernet_get_header(uint8_t* buffer) {
    ethernet_header_t header = {0};

    memcpy((uint8_t*)&header, buffer, ETHERNET_HEADER_LEN);

    return header;
}
