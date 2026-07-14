#include <stdint.h>
#include "subnet_math.h"

/**
 * @brief convert mask to cidr
 * 
 * @param cidr 
 * @return uint32_t 
 */
uint8_t count_number_of_continuous_ones(uint32_t subnet_number) {
    uint8_t cidr = 0;
    uint32_t copy_subnet_number = subnet_number;
    for(short i = 0; i < 33; i++) {
        if(copy_subnet_number & 0x80000000) {
            cidr += 1;
            copy_subnet_number = copy_subnet_number << 1;
        } else {
            break; // need to break since subnet should only count continuous series of 1s.
        }
    }
    return cidr;
}

/**
 * @brief get subnet detials from IP and CIDR.
 * This function will also calculate edge case of the /31, /32 subnets
 * 
 * @param ip 
 * @param cidr 
 * @param out_network 
 * @param out_broadcast 
 * @param out_host_count 
 */
void subnet_calculate(
    const uint8_t ip[4],
    uint8_t cidr,
    uint32_t* out_network, // network address
    uint32_t* out_broadcast, // broadcast address
    uint32_t* out_host_count, // usable hosts (broadcast - network - 1)
    uint32_t* out_subnet_mask) {
    uint32_t mask = cidr_to_mask(cidr);
    uint32_t ip_number = ip_to_u32(ip);
    *out_subnet_mask = mask;
    *out_network = mask & ip_number;
    switch(cidr) {
    case 31:
        *out_broadcast = *out_network + 1;
        *out_host_count = 2;
        break;
    case 32:
        *out_broadcast = *out_network;
        *out_host_count = 0;
        break;
    default:
        *out_broadcast = ip_number | (~mask);
        *out_host_count = *out_broadcast - *out_network;
    }
}
