#pragma once
/*
 * radio_hw.h — CC1101 hardware abstraction for RS41 Tracker
 *
 * Supports two backends:
 *   RadioInternal — built-in CC1101 via furi_hal_subghz_* + direct SPI
 *   RadioExternal — external CC1101 module on the Flipper GPIO SPI header
 *
 * External module wiring (Flipper Zero GPIO header):
 *   CC1101 VCC  → Pin 3  (3.3V)
 *   CC1101 GND  → Pin 2  (GND)
 *   CC1101 MOSI → Pin 12 (PA7 / SPI1_MOSI)
 *   CC1101 MISO → Pin 13 (PA6 / SPI1_MISO = CC1101 GDO1)
 *   CC1101 SCK  → Pin 15 (PB3 / SPI1_CLK)
 *   CC1101 CSN  → Pin 14 (PA4 / SPI1_NSS)
 *   CC1101 GDO0 → Pin 7  (PC3 / GPIO, optional — not used by firmware)
 */

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RadioInternal = 0, /* built-in CC1101 (default)           */
    RadioExternal = 1, /* external module on GPIO SPI header  */
} RadioSource;

/*
 * Load CC1101 preset: reset chip, write all config registers, load PA table.
 * preset must be in the same {reg,val}...{0,0}+pa[8] format as
 * furi_hal_subghz_load_custom_preset().
 */
void radio_hw_init(RadioSource src, const uint8_t* preset);

/*
 * Tune to freq_hz.  For internal this also selects the correct RF path;
 * for external it writes FREQ2/FREQ1/FREQ0 directly.
 */
void radio_hw_set_frequency(RadioSource src, uint32_t freq_hz);

/* Switch CC1101 to RX mode (must be IDLE first). */
void radio_hw_rx(RadioSource src);

/* Switch CC1101 to IDLE state. */
void radio_hw_idle(RadioSource src);

/* Put CC1101 to sleep / power-down. */
void radio_hw_sleep(RadioSource src);

/* Flush RX FIFO (SFRX strobe). */
void radio_hw_flush_rx(RadioSource src);

/*
 * Read RXBYTES status register.
 * bit 7 = RXFIFO_OVERFLOW flag, bits 6:0 = bytes currently in FIFO.
 */
uint8_t radio_hw_rxbytes(RadioSource src);

/* Read instantaneous RSSI in dBm. */
float radio_hw_get_rssi(RadioSource src);

/*
 * Burst-read exactly n bytes from the CC1101 RX FIFO into dst.
 * Bypasses the firmware bug that consumes the first byte as a length field.
 */
void radio_hw_read_fifo(RadioSource src, uint8_t* dst, uint8_t n);
