#include "arp.h"
#include "ethernet_protocol.h"

bool arp_set_header(
    uint8_t* buffer,
    uint8_t* MAC_SRC,
    uint8_t* MAC_DES,
    uint8_t* IP_SRC,
    uint8_t* IP_DES,
    uint16_t opcode) {
    if(buffer == NULL || MAC_SRC == NULL || MAC_DES == NULL || IP_SRC == NULL || IP_DES == NULL)
        return false;

    // set the range
    arp_header_t* header = (arp_header_t*)buffer;

    // initialize the header
    memset(header, 0, sizeof(*header));

    // set the hardware type on ethernet
    header->hardware_type[0] = 0;
    header->hardware_type[1] = 1;

    // set the type of protocol in IPV4
    header->protocol_type[0] = 0x08;
    header->protocol_type[1] = 0x0;

    // size of hardware protocol
    header->hardware_length = 6;

    // size of protocol (ipv4 in this case)
    header->protocol_length = 4;

    // Set opcode
    header->operation_code[0] = opcode >> 8 & 0xff;
    header->operation_code[1] = opcode & 0xff;

    // set the ip source
    memcpy(header->ip_source, IP_SRC, 4);

    // set the ip destiny
    memcpy(header->ip_destiny, IP_DES, 4);

    // set the mac source
    memcpy(header->mac_source, MAC_SRC, 6);

    // set the mac destiny
    memcpy(header->mac_destiny, MAC_DES, 6);

    return true;
}

bool is_arp(uint8_t* buffer) {
    if(buffer == NULL) return false;

    ethernet_header_t header = ethernet_get_header(buffer);

    uint16_t type = header.type[0] << 8 | header.type[1];

    return type == 0x0806;
}

arp_header_t arp_get_header(uint8_t* buffer) {
    arp_header_t arp_header = {0};

    if(buffer == NULL) return arp_header;

    memcpy((uint8_t*)&arp_header, buffer + ETHERNET_HEADER_LEN, ARP_LEN);

    return arp_header;
}
