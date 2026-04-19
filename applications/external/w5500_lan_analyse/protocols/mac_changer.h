#pragma once

#include <stdint.h>

/* Default MAC address (WIZnet OUI) */
#define MAC_CHANGER_DEFAULT_MAC {0x00, 0x08, 0xDC, 0x47, 0x47, 0x54}

/**
 * Generate a random locally-administered MAC address.
 * Sets bit 1 of first byte (locally administered) and clears bit 0 (unicast).
 * mac: output 6-byte MAC
 */
void mac_changer_generate_random(uint8_t mac[6]);
