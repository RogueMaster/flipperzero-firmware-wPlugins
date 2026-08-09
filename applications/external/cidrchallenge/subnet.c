#include "subnet.h"

#include <stdio.h>

uint32_t subnet_mask(uint8_t prefix) {
    if(prefix == 0) return 0;
    if(prefix >= 32) return 0xFFFFFFFFu;
    return 0xFFFFFFFFu << (32 - prefix);
}

uint32_t subnet_network(uint32_t ip, uint8_t prefix) {
    return ip & subnet_mask(prefix);
}

uint32_t subnet_broadcast(uint32_t ip, uint8_t prefix) {
    return ip | ~subnet_mask(prefix);
}

uint32_t subnet_first_host(uint32_t ip, uint8_t prefix) {
    uint32_t network = subnet_network(ip, prefix);
    if(prefix >= 31) return network;
    return network + 1;
}

uint32_t subnet_last_host(uint32_t ip, uint8_t prefix) {
    uint32_t broadcast = subnet_broadcast(ip, prefix);
    if(prefix >= 31) return broadcast;
    return broadcast - 1;
}

uint32_t subnet_block_size(uint8_t prefix) {
    if(prefix == 0) return 0xFFFFFFFFu;
    if(prefix >= 32) return 1;
    return 1u << (32 - prefix);
}

uint32_t subnet_usable_hosts(uint8_t prefix) {
    if(prefix >= 32) return 1;
    if(prefix == 31) return 2;
    return subnet_block_size(prefix) - 2;
}

uint8_t subnet_prefix_for_hosts(uint32_t hosts) {
    for(uint8_t prefix = 30; prefix > 8; prefix--) {
        if(subnet_usable_hosts(prefix) >= hosts) return prefix;
    }
    return 8;
}

uint8_t subnet_prefix_from_mask(uint32_t mask) {
    for(uint8_t prefix = 0; prefix <= 32; prefix++) {
        if(subnet_mask(prefix) == mask) return prefix;
    }
    return 255;
}

void subnet_format_ip(uint32_t ip, char* out, size_t out_size) {
    snprintf(
        out,
        out_size,
        "%lu.%lu.%lu.%lu",
        (unsigned long)((ip >> 24) & 0xFF),
        (unsigned long)((ip >> 16) & 0xFF),
        (unsigned long)((ip >> 8) & 0xFF),
        (unsigned long)(ip & 0xFF));
}
