#pragma once

#include <stdint.h>
#include <stdbool.h>

/* MAC config file path */
#define MAC_CONFIG_PATH APP_DATA_PATH("mac.conf")

/* Default MAC address (WIZnet OUI) */
#define MAC_CHANGER_DEFAULT_MAC {0x00, 0x08, 0xDC, 0x47, 0x47, 0x54}

/**
 * Generate a random locally-administered MAC address.
 * Sets bit 1 of first byte (locally administered) and clears bit 0 (unicast).
 * mac: output 6-byte MAC
 */
void mac_changer_generate_random(uint8_t mac[6]);

/**
 * Save MAC address to SD card config file.
 * Returns true on success.
 */
bool mac_changer_save(const uint8_t mac[6]);

/**
 * Load MAC address from SD card config file.
 * Returns true if valid MAC was loaded.
 */
bool mac_changer_load(uint8_t mac[6]);

/**
 * Delete MAC config file (reset to default).
 * Returns true on success.
 */
bool mac_changer_delete_config(void);
