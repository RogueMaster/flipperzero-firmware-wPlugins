/**
 * CC1101 packet radio access for Ducobox RF.
 *
 * The Ducobox uses GFSK at 868.294 MHz, 38.3835 kBaud, 20.63 kHz deviation
 * and sync word 0xD391. The values below come from the reverse engineered
 * Ducobox-ESPEasy-Plugin CC1101 configuration. The CC1101 packet engine does
 * sync detection, length handling and CRC, so only payload bytes are moved
 * in and out of the RX/TX FIFO here.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define DUCO_MAX_PACKET 64

/** Configure the radio for the given frequency and enter RX. */
void duco_radio_start_rx(uint32_t frequency);

/** Retune the already running receiver. */
void duco_radio_retune(uint32_t frequency);

/** Current channel RSSI in dBm, rounded to a whole number. */
int16_t duco_radio_get_rssi(void);

/**
 * Poll for a received packet.
 *
 * Returns false when no complete packet is available. On success, size is
 * the payload length, data holds the payload without length byte or CRC, and
 * crc_ok reports the CC1101 CRC result. Frames with a bad CRC are returned
 * too, so callers can show them and debug reception.
 */
bool duco_radio_read_packet(uint8_t* data, uint8_t* size, bool* crc_ok);

/** Leave RX and put the radio into idle. */
void duco_radio_stop(void);

/** Transmit one payload. The CC1101 appends the length byte and CRC. */
bool duco_radio_send_packet(uint32_t frequency, const uint8_t* data, uint8_t size);
