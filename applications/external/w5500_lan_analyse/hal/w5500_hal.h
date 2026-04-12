#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * W5500 Hardware Abstraction Layer
 *
 * Pin mapping (W5500 Lite -> Flipper GPIO):
 *   MOSI (MO)   -> A7  (pin 2)
 *   SCLK (SCK)  -> B3  (pin 5)
 *   nSS  (CS)   -> A4  (pin 4)
 *   MISO (MI)   -> A6  (pin 3)
 *   RESET (RST) -> C3  (pin 7)
 *   3V3  (VCC)  -> 3V3 (pin 9)
 *   GND  (G)    -> GND (pin 8 or 11)
 */

/* MACRAW socket is always Socket 0 on W5500 */
#define W5500_MACRAW_SOCKET 0

/* DHCP uses Socket 1 */
#define W5500_DHCP_SOCKET 1

/* Ping uses Socket 2 */
#define W5500_PING_SOCKET 2

/* PHY configuration register address */
#define W5500_PHYCFGR_ADDR 0x002E

/* PHY link status, speed, duplex from PHYCFGR */
#define PHYCFGR_LNK_MASK 0x01 /* Bit 0: link status */
#define PHYCFGR_SPD_MASK 0x02 /* Bit 1: speed (0=10M, 1=100M) */
#define PHYCFGR_DPX_MASK 0x04 /* Bit 2: duplex (0=half, 1=full) */

/**
 * Initialize SPI bus, reset pin, CS pin.
 * Enables OTG power for the W5500 module.
 * Returns true on success.
 */
bool w5500_hal_init(void);

/**
 * Deinitialize: release SPI, disable OTG power, reset GPIOs.
 */
void w5500_hal_deinit(void);

/**
 * Perform hardware reset of W5500 via RST pin (C3).
 * Waits for the chip to become ready.
 */
void w5500_hal_hw_reset(void);

/**
 * Initialize the W5500 chip: set FIFO sizes, register SPI callbacks.
 * Must be called after w5500_hal_init() and w5500_hal_hw_reset().
 * Returns true on success.
 */
bool w5500_hal_chip_init(void);

/**
 * Check W5500 VERSIONR register (should be 0x04).
 * Returns true if the chip responds correctly.
 */
bool w5500_hal_check_version(void);

/**
 * Set MAC address on the W5500.
 */
void w5500_hal_set_mac(const uint8_t mac[6]);

/**
 * Get MAC address currently set on W5500.
 */
void w5500_hal_get_mac(uint8_t mac[6]);

/**
 * Set network configuration (IP, subnet, gateway, DNS).
 */
void w5500_hal_set_net_info(
    const uint8_t ip[4],
    const uint8_t subnet[4],
    const uint8_t gateway[4],
    const uint8_t dns[4]);

/**
 * Read PHY link status.
 * Returns true if link is up.
 */
bool w5500_hal_get_link_status(void);

/**
 * Read PHYCFGR register and extract speed/duplex.
 * speed: 0 = 10 Mbps, 1 = 100 Mbps
 * duplex: 0 = half, 1 = full
 */
void w5500_hal_get_phy_info(bool* link_up, uint8_t* speed, uint8_t* duplex);

/**
 * Open Socket 0 in MACRAW mode with MFEN=0 (accept all frames).
 * Returns true on success.
 */
bool w5500_hal_open_macraw(void);

/**
 * Close MACRAW socket.
 */
void w5500_hal_close_macraw(void);

/**
 * Receive a raw Ethernet frame from MACRAW socket.
 * buf: output buffer
 * buf_size: max buffer size
 * Returns number of bytes received, 0 if no data.
 */
uint16_t w5500_hal_macraw_recv(uint8_t* buf, uint16_t buf_size);

/**
 * Send a raw Ethernet frame via MACRAW socket.
 * Returns number of bytes sent, 0 on error.
 */
uint16_t w5500_hal_macraw_send(const uint8_t* buf, uint16_t len);
