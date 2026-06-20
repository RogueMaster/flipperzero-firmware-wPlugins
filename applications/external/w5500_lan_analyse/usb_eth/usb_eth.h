#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * USB CDC-ECM (Ethernet Control Model) Network Device
 *
 * Turns the Flipper Zero into a USB Ethernet adapter.
 * The host (phone/PC) sees a standard network interface.
 *
 * Protocol: CDC-ECM (natively supported on Linux, macOS, Android)
 */

/**
 * Initialize the USB CDC-ECM interface.
 * Saves the current USB config and switches to the ECM network device.
 * Returns true on success.
 */
bool usb_eth_init(void);

/**
 * Deinitialize: restore the previous USB interface (CDC Serial).
 */
void usb_eth_deinit(void);

/**
 * Send an Ethernet frame to the USB host.
 * frame: raw Ethernet frame (dest MAC + src MAC + ethertype + payload)
 * len: frame length (14..1518 bytes)
 * Returns true on success.
 */
bool usb_eth_send_frame(const uint8_t* frame, uint16_t len);

/**
 * Receive an Ethernet frame from the USB host (non-blocking).
 * frame: output buffer
 * max_len: buffer size (should be >= 1518)
 * Returns number of bytes received, 0 if no data available.
 */
int16_t usb_eth_receive_frame(uint8_t* frame, uint16_t max_len);

/**
 * Check if the USB host has configured the ECM interface.
 * Returns true if a host is connected and the interface is active.
 */
bool usb_eth_is_connected(void);
