#include "mac_changer.h"

#include <furi.h>
#include <furi_hal_random.h>

void mac_changer_generate_random(uint8_t mac[6]) {
    furi_hal_random_fill_buf(mac, 6);
    /* Set locally administered bit (bit 1 of first byte) */
    mac[0] |= 0x02;
    /* Clear multicast bit (bit 0 of first byte) */
    mac[0] &= 0xFE;
}
